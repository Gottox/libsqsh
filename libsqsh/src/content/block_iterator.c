/******************************************************************************
 *                                                                            *
 * Copyright (c) 2023-2024, Enno Boland <g@s01.de>                            *
 *                                                                            *
 * Redistribution and use in source and binary forms, with or without         *
 * modification, are permitted provided that the following conditions are     *
 * met:                                                                       *
 *                                                                            *
 * * Redistributions of source code must retain the above copyright notice,   *
 *   this list of conditions and the following disclaimer.                    *
 * * Redistributions in binary form must reproduce the above copyright        *
 *   notice, this list of conditions and the following disclaimer in the      *
 *   documentation and/or other materials provided with the distribution.     *
 *                                                                            *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS    *
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,  *
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR     *
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR          *
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,      *
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,        *
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR         *
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF     *
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING       *
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS         *
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.               *
 *                                                                            *
 ******************************************************************************/

/**
 * @author       Enno Boland (mail@eboland.de)
 * @file         block_iterator.c
 */

#include <sqsh_content_private.h>

#include <cextras/collection.h>
#include <sqsh_archive_private.h>
#include <sqsh_common_private.h>
#include <sqsh_error.h>
#include <sqsh_file_private.h>
#include <stdint.h>

#define BLOCK_INDEX_FINISHED UINT32_MAX

static bool
is_last_block(const struct SqshBlockIterator *iterator) {
	const struct SqshFile *file = iterator->file;
	const bool has_fragment = sqsh_file_has_fragment(file);
	const uint64_t block_index = iterator->block_index;
	const uint64_t block_count = sqsh_file_block_count2(file);

	if (has_fragment) {
		return block_index == block_count;
	} else {
		return block_index == block_count - 1;
	}
}

int
sqsh__block_iterator_init(
		struct SqshBlockIterator *iterator, const struct SqshFile *file) {
	int rv = 0;
	enum SqshFileType file_type = sqsh_file_type(file);
	if (file_type != SQSH_FILE_TYPE_FILE) {
		rv = -SQSH_ERROR_NOT_A_FILE;
		goto out;
	}

	struct SqshArchive *archive = file->archive;
	const struct SqshSuperblock *superblock = sqsh_archive_superblock(archive);
	struct SqshMapManager *map_manager = sqsh_archive_map_manager(archive);
	uint64_t block_address = sqsh_file_blocks_start(file);
	const uint64_t upper_limit = sqsh_superblock_bytes_used(superblock);

	rv = sqsh__archive_data_extract_manager(
			archive, &iterator->compression_manager);
	if (rv < 0) {
		goto out;
	}
	rv = sqsh__map_reader_init(
			&iterator->map_reader, map_manager, block_address, upper_limit);
	if (rv < 0) {
		goto out;
	}

	iterator->block_index = 0;
	iterator->block_size = sqsh_superblock_block_size(superblock);
	iterator->file = file;
	iterator->sparse_size = 0;
out:
	return rv;
}

int
sqsh__block_iterator_copy(
		struct SqshBlockIterator *target,
		const struct SqshBlockIterator *source) {
	int rv = 0;
	target->file = source->file;
	target->compression_manager = source->compression_manager;
	rv = sqsh__map_reader_copy(&target->map_reader, &source->map_reader);
	if (rv < 0) {
		goto out;
	}
	rv = sqsh__extract_view_copy(&target->extract_view, &source->extract_view);
	if (rv < 0) {
		goto out;
	}
	rv = sqsh__fragment_view_copy(
			&target->fragment_view, &source->fragment_view);
	if (rv < 0) {
		goto out;
	}
	target->sparse_size = source->sparse_size;
	target->block_size = source->block_size;
	target->block_index = source->block_index;
	target->data = source->data;
	target->size = source->size;
out:
	if (rv < 0) {
		sqsh__block_iterator_cleanup(target);
	}
	return rv;
}

static int
map_block_compressed(
		struct SqshBlockIterator *iterator, sqsh_index_t next_offset) {
	int rv = 0;
	struct SqshExtractManager *compression_manager =
			iterator->compression_manager;
	const struct SqshFile *file = iterator->file;
	struct SqshExtractView *extract_view = &iterator->extract_view;
	const uint64_t block_index = iterator->block_index;
	const uint32_t data_block_size = sqsh_file_block_size2(file, block_index);

	rv = sqsh__map_reader_advance(
			&iterator->map_reader, next_offset, data_block_size);
	if (rv < 0) {
		goto out;
	}
	rv = sqsh__extract_view_init(
			extract_view, compression_manager, &iterator->map_reader);
	if (rv < 0) {
		goto out;
	}
	iterator->data = sqsh__extract_view_data(extract_view);
	iterator->size = sqsh__extract_view_size(extract_view);

	if (SQSH_ADD_OVERFLOW(block_index, 1, &iterator->block_index)) {
		rv = -SQSH_ERROR_INTEGER_OVERFLOW;
		goto out;
	}

	rv = 1;
out:
	return rv;
}

static int
map_block_uncompressed(
		struct SqshBlockIterator *iterator, sqsh_index_t next_offset,
		size_t desired_size) {
	int rv = 0;
	const struct SqshFile *file = iterator->file;
	uint64_t block_index = iterator->block_index;
	struct SqshMapReader *reader = &iterator->map_reader;
	const uint64_t block_count = sqsh_file_block_count2(file);
	size_t outer_size = 0;
	const size_t remaining_direct = sqsh__map_reader_remaining_direct(reader);

	for (; iterator->sparse_size == 0 && block_index < block_count;
		 block_index++) {
		if (sqsh_file_block_is_compressed2(file, block_index)) {
			break;
		}
		if (outer_size >= desired_size) {
			break;
		}
		const uint32_t data_block_size =
				sqsh_file_block_size2(file, block_index);
		/* Set the sparse size only if we are not at the last block. */
		if (block_index + 1 != block_count) {
			iterator->sparse_size = iterator->block_size - data_block_size;
		}

		size_t new_outer_size;
		if (SQSH_ADD_OVERFLOW(outer_size, data_block_size, &new_outer_size)) {
			rv = -SQSH_ERROR_INTEGER_OVERFLOW;
			goto out;
		}

		/* To avoid crossing mem block boundaries, we stop
		 * if the next block would cross the boundary. The only exception
		 * is that we need to map at least one block.
		 */
		if (new_outer_size > remaining_direct && outer_size > 0) {
			break;
		}
		outer_size = new_outer_size;
	}
	rv = sqsh__map_reader_advance(reader, next_offset, outer_size);
	if (rv < 0) {
		goto out;
	}
	iterator->data = sqsh__map_reader_data(reader);
	iterator->size = outer_size;
	iterator->block_index = block_index;

	rv = 1;
out:
	return rv;
}

static int
map_zero_block(struct SqshBlockIterator *iterator) {
	const struct SqshFile *file = iterator->file;
	const struct SqshArchive *archive = file->archive;
	const size_t zero_block_size = sqsh__archive_zero_block_size(archive);

	size_t current_sparse_size = iterator->sparse_size;

	if (current_sparse_size > zero_block_size) {
		current_sparse_size = zero_block_size;
	}
	iterator->sparse_size -= current_sparse_size;

	iterator->size = current_sparse_size;
	iterator->data = sqsh__archive_zero_block(archive);

	return 1;
}

static int
map_block(struct SqshBlockIterator *iterator, size_t desired_size) {
	int rv = 0;
	const struct SqshFile *file = iterator->file;

	const uint64_t block_index = iterator->block_index;
	const size_t block_size = iterator->block_size;
	const bool is_compressed =
			sqsh_file_block_is_compressed2(file, block_index);
	const uint64_t file_size = sqsh_file_size(file);
	const size_t data_block_size = sqsh_file_block_size2(file, block_index);
	const sqsh_index_t next_offset =
			sqsh__map_reader_size(&iterator->map_reader);

	if (data_block_size == 0) {
		if (is_last_block(iterator) == false || file_size % block_size == 0) {
			iterator->sparse_size = block_size;
		} else {
			iterator->sparse_size = (size_t)file_size % block_size;
		}
		rv = map_zero_block(iterator);
		iterator->block_index++;
	} else if (is_compressed) {
		rv = map_block_compressed(iterator, next_offset);
	} else {
		rv = map_block_uncompressed(iterator, next_offset, desired_size);
	}
	return rv;
}

static int
map_fragment(struct SqshBlockIterator *iterator) {
	int rv = 0;
	const struct SqshFile *file = iterator->file;
	struct SqshArchive *archive = file->archive;
	struct SqshFragmentTable *fragment_table = NULL;
	struct SqshFragmentView *fragment_view = &iterator->fragment_view;
	rv = sqsh_archive_fragment_table(archive, &fragment_table);
	if (rv < 0) {
		goto out;
	}

	rv = sqsh__fragment_view_init(fragment_view, file);
	if (rv < 0) {
		goto out;
	}
	iterator->data = sqsh__fragment_view_data(fragment_view);
	iterator->size = sqsh__fragment_view_size(fragment_view);
	iterator->block_index = BLOCK_INDEX_FINISHED;
out:
	if (rv < 0) {
		return rv;
	} else {
		return 1;
	}
}

bool
sqsh__block_iterator_is_zero_block(const struct SqshBlockIterator *iterator) {
	return iterator->sparse_size > 0;
}

bool
sqsh__block_iterator_next(
		struct SqshBlockIterator *iterator, size_t desired_size, int *err) {
	int rv = 0;
	const struct SqshFile *file = iterator->file;
	const uint64_t block_count = sqsh_file_block_count2(file);
	const bool has_fragment = sqsh_file_has_fragment(file);
	bool has_next = true;

	sqsh__extract_view_cleanup(&iterator->extract_view);

	// Desired size of 0 would result in a noop for uncompressed blocks,
	// resulting in inconsistend behavior depending whether the block is
	// compressed, which ignores the desired size, or uncompressed, which
	// honors the desired size.
	if (desired_size == 0) {
		desired_size = 1;
	}

	if (sqsh__block_iterator_is_zero_block(iterator)) {
		rv = map_zero_block(iterator);
	} else if (iterator->block_index < block_count) {
		rv = map_block(iterator, desired_size);
	} else if (has_fragment && iterator->block_index == block_count) {
		rv = map_fragment(iterator);
	} else {
		iterator->data = NULL;
		iterator->size = 0;
		rv = 0;
		has_next = false;
	}

	if (err != NULL) {
		*err = rv;
	}
	if (rv < 0) {
		has_next = false;
	}
	return has_next;
}

int
sqsh__block_iterator_skip(
		struct SqshBlockIterator *iterator, uint64_t *offset,
		size_t desired_size) {
	int rv = 0;
	const size_t block_size = iterator->block_size;
	const size_t current_block_size = sqsh__block_iterator_size(iterator);

	if (*offset < current_block_size) {
		goto out;
	}

	*offset -= current_block_size;

	uint64_t skip_index = *offset / block_size;
	if (current_block_size != 0) {
		skip_index += 1;
	}

	*offset = *offset % block_size;

	if (skip_index == 0 && iterator->block_index != 0) {
		goto out;
	}

	sqsh_index_t reader_forward = 0;
	uint64_t block_index = iterator->block_index;
	const uint64_t block_count = sqsh_file_block_count2(iterator->file);
	for (sqsh_index_t i = 0; i < skip_index && block_index < block_count; i++) {
		reader_forward += sqsh_file_block_size2(iterator->file, block_index);
		block_index += 1;
	}
	rv = sqsh__map_reader_advance(&iterator->map_reader, reader_forward, 0);
	if (rv < 0) {
		goto out;
	}
	iterator->sparse_size = 0;
	iterator->block_index = block_index;

	/* In general we will get directly the block containing the offset, but if
	 * the offset points to a block with sparse sections, we iterate over them
	 * until we reach the desired offset.
	 *
	 * TODO: We could analyze iterator->sparse_size and directly skip to the
	 *       desired block.
	 */
	size_t current_size = sqsh__block_iterator_size(iterator);
	while (current_size <= *offset) {
		*offset -= current_size;
		bool has_next = sqsh__block_iterator_next(iterator, desired_size, &rv);
		if (rv < 0) {
			goto out;
		} else if (!has_next) {
			rv = -SQSH_ERROR_OUT_OF_BOUNDS;
			goto out;
		}
		current_size = sqsh__block_iterator_size(iterator);
	}

	rv = 0;

out:
	return rv;
}

const uint8_t *
sqsh__block_iterator_data(const struct SqshBlockIterator *iterator) {
	return iterator->data;
}

size_t
sqsh__block_iterator_block_size(const struct SqshBlockIterator *iterator) {
	return iterator->block_size;
}

size_t
sqsh__block_iterator_size(const struct SqshBlockIterator *iterator) {
	return iterator->size;
}

int
sqsh__block_iterator_cleanup(struct SqshBlockIterator *iterator) {
	sqsh__map_reader_cleanup(&iterator->map_reader);
	sqsh__extract_view_cleanup(&iterator->extract_view);
	sqsh__fragment_view_cleanup(&iterator->fragment_view);
	return 0;
}
