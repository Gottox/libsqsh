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
 * @file         file_iterator.c
 */

#include <sqsh_archive_private.h>
#include <sqsh_common_private.h>
#include <sqsh_content_private.h>
#include <sqsh_extract_private.h>
#include <sqsh_file_private.h>

#include <sqsh_error.h>

int
sqsh__block_iterator_init(
		struct SqshBlockIterator *iterator, const struct SqshFile *file) {
	int rv = 0;
	struct SqshArchive *archive = file->archive;
	const struct SqshSuperblock *superblock = sqsh_archive_superblock(archive);
	const uint32_t block_size = sqsh_superblock_block_size(superblock);

	rv = sqsh__archive_data_extract_manager(
			archive, &iterator->compression_manager);
	if (rv < 0) {
		goto out;
	}
	iterator->block_size = block_size;
	iterator->file = file;
	iterator->block_index = SIZE_MAX;
	iterator->block_count = sqsh_file_block_count2(file);
	iterator->sparse_size = 0;
	iterator->data = NULL;
	iterator->size = 0;
	if (iterator->block_count == UINT64_MAX) {
		rv = -SQSH_ERROR_CORRUPTED_INODE;
		goto out;
	}

out:
	sqsh__block_iterator_cleanup(iterator);
	return 0;
}

int
sqsh__block_iterator_copy(
		struct SqshBlockIterator *target,
		const struct SqshBlockIterator *source) {
	int rv = 0;
	target->file = source->file;
	target->compression_manager = source->compression_manager;
	target->block_index = source->block_index;
	target->block_count = source->block_count;
	target->sparse_size = source->sparse_size;
	target->size = source->size;

	rv = sqsh__map_reader_copy(&target->map_reader, &source->map_reader);
	if (rv < 0) {
		goto out;
	}
	rv = sqsh__extract_view_copy(&target->extract_view, &source->extract_view);
	if (rv < 0) {
		goto out;
	}

	const uint8_t *map_data = sqsh__map_reader_data(&source->map_reader);
	const uint8_t *extract_data =
			sqsh__extract_view_data(&source->extract_view);
	const uint8_t *source_data = source->data;

	if (source_data == map_data) {
		target->data = sqsh__map_reader_data(&target->map_reader);
	} else if (source_data == extract_data) {
		target->data = sqsh__extract_view_data(&target->extract_view);
	} else {
		target->data = source_data;
	}

out:
	if (rv < 0) {
		sqsh__block_iterator_cleanup(target);
	}
	return rv;
}

static int
map_block_sparse(struct SqshBlockIterator *iterator) {
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
map_block_compressed(
		struct SqshBlockIterator *iterator, sqsh_index_t next_offset) {
	int rv = 0;
	struct SqshExtractManager *compression_manager =
			iterator->compression_manager;
	const struct SqshFile *file = iterator->file;
	struct SqshExtractView *extract_view = &iterator->extract_view;
	const uint64_t block_index = iterator->block_index;
	const sqsh_index_t data_block_size =
			sqsh_file_block_size2(file, block_index);

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
map_block(struct SqshBlockIterator *iterator, size_t desired_size) {
	int rv = 0;
	const struct SqshFile *file = iterator->file;

	const uint32_t block_size = iterator->block_size;
	const uint64_t block_index = iterator->block_index;
	const uint64_t block_count = iterator->block_count;
	const bool is_compressed =
			sqsh_file_block_is_compressed2(file, block_index);
	const uint64_t file_size = sqsh_file_size(file);
	const size_t data_block_size = sqsh_file_block_size2(file, block_index);
	const sqsh_index_t next_offset =
			sqsh__map_reader_size(&iterator->map_reader);

	if (data_block_size == 0) {
		if (block_index != block_count - 1 || file_size % block_size == 0) {
			iterator->sparse_size = block_size;
		} else {
			iterator->sparse_size = (size_t)file_size % block_size;
		}
		rv = map_block_sparse(iterator);
	} else if (is_compressed) {
		rv = map_block_compressed(iterator, next_offset);
	} else {
		rv = map_block_uncompressed(iterator, next_offset, desired_size);
	}
	return rv;
}

bool
sqsh__block_iterator_next(
		struct SqshBlockIterator *iterator, size_t desired_size, int *err) {
	int rv = 0;
	bool has_next = true;

	sqsh__extract_view_cleanup(&iterator->extract_view);

	if (iterator->sparse_size == 0) {
		iterator->block_index++;
	}

	if (desired_size < 1) {
		rv = -SQSH_ERROR_INVALID_ARGUMENT;
		goto out;
	}

	if (sqsh__block_iterator_is_zero_block(iterator)) {
		rv = map_block_sparse(iterator);
	} else if (iterator->block_index < iterator->block_count) {
		rv = map_block(iterator, desired_size);
	} else {
		has_next = false;
	}

out:
	if (rv < 0) {
		*err = rv;
		return false;
	}
	return has_next;
}

bool
sqsh__block_iterator_is_zero_block(const struct SqshBlockIterator *iterator) {
	return iterator->sparse_size > 0;
}

int
sqsh__block_iterator_skip_nomap(
		struct SqshBlockIterator *iterator, uint64_t *offset,
		size_t desired_size) {
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

	iterator->block_index += skip_index - 1;

out:
	return 0;
}

const uint8_t *
sqsh__block_iterator_data(const struct SqshBlockIterator *iterator) {
	return iterator->data;
}

size_t
sqsh__block_iterator_size(const struct SqshBlockIterator *iterator) {
	return iterator->size;
}

int
sqsh__block_iterator_cleanup(struct SqshBlockIterator *iterator) {
	sqsh__map_reader_cleanup(&iterator->map_reader);
	sqsh__extract_view_cleanup(&iterator->extract_view);
	return 0;
}
