/******************************************************************************
 *                                                                            *
 * Copyright (c) 2023, Enno Boland <g@s01.de>                                 *
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

#include "../../include/sqsh_file_private.h"

#include "../../include/sqsh_archive_private.h"
#include "../../include/sqsh_error.h"
#include "../utils/utils.h"
#include <cextras/collection.h>

#define BLOCK_INDEX_FINISHED UINT32_MAX

static const uint8_t ZERO_BLOCK[16384] = {0};

static bool
is_last_block(const struct SqshFileIterator *iterator) {
	const struct SqshFile *file = iterator->file;
	const bool has_fragment = sqsh_file_has_fragment(file);
	const sqsh_index_t block_index = iterator->block_index;
	const size_t block_count = sqsh_file_block_count(file);

	if (has_fragment) {
		return block_index == block_count;
	} else {
		return block_index == block_count - 1;
	}
}

int
sqsh__file_iterator_init(
		struct SqshFileIterator *iterator, const struct SqshFile *file) {
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

struct SqshFileIterator *
sqsh_file_iterator_new(const struct SqshFile *file, int *err) {
	int rv = 0;
	struct SqshFileIterator *iterator =
			calloc(1, sizeof(struct SqshFileIterator));
	if (iterator == NULL) {
		rv = -SQSH_ERROR_MALLOC_FAILED;
		goto out;
	}
	rv = sqsh__file_iterator_init(iterator, file);
	if (rv < 0) {
		free(iterator);
		iterator = NULL;
	}
out:
	if (err != NULL) {
		*err = rv;
	}
	return iterator;
}

static int
map_block_compressed(
		struct SqshFileIterator *iterator, sqsh_index_t next_offset) {
	int rv = 0;
	struct SqshExtractManager *compression_manager =
			iterator->compression_manager;
	const struct SqshFile *file = iterator->file;
	struct SqshExtractView *extract_view = &iterator->extract_view;
	const sqsh_index_t block_index = iterator->block_index;
	const sqsh_index_t data_block_size =
			sqsh_file_block_size(file, block_index);

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

	rv = sqsh_file_iterator_size(iterator);
out:
	return rv;
}

static int
map_block_uncompressed(
		struct SqshFileIterator *iterator, sqsh_index_t next_offset,
		size_t desired_size) {
	int rv = 0;
	const struct SqshFile *file = iterator->file;
	sqsh_index_t block_index = iterator->block_index;
	struct SqshMapReader *reader = &iterator->map_reader;
	const uint64_t block_count = sqsh_file_block_count(file);
	uint64_t outer_size = 0;
	const size_t remaining_direct = sqsh__map_reader_remaining_direct(reader);

	for (; iterator->sparse_size == 0 && block_index < block_count;
		 block_index++) {
		if (sqsh_file_block_is_compressed(file, block_index)) {
			break;
		}
		if (outer_size >= desired_size) {
			break;
		}
		const uint32_t data_block_size =
				sqsh_file_block_size(file, block_index);
		/* Set the sparse size only if we are not at the last block. */
		if (block_index + 1 != block_count) {
			iterator->sparse_size = iterator->block_size - data_block_size;
		}

		uint64_t new_outer_size;
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
map_zero_block(struct SqshFileIterator *iterator) {
	size_t current_sparse_size = iterator->sparse_size;

	if (current_sparse_size > sizeof(ZERO_BLOCK)) {
		current_sparse_size = sizeof(ZERO_BLOCK);
	}
	iterator->sparse_size -= current_sparse_size;

	iterator->size = current_sparse_size;
	iterator->data = ZERO_BLOCK;

	return current_sparse_size;
}

static int
map_block(struct SqshFileIterator *iterator, size_t desired_size) {
	int rv = 0;
	const struct SqshFile *file = iterator->file;

	const sqsh_index_t block_index = iterator->block_index;
	const size_t block_size = iterator->block_size;
	const bool is_compressed = sqsh_file_block_is_compressed(file, block_index);
	const size_t file_size = sqsh_file_size(file);
	const size_t data_block_size = sqsh_file_block_size(file, block_index);
	const sqsh_index_t next_offset =
			sqsh__map_reader_size(&iterator->map_reader);

	if (data_block_size == 0) {
		if (is_last_block(iterator) == false || file_size == block_size) {
			iterator->sparse_size = block_size;
		} else {
			iterator->sparse_size = file_size % block_size;
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
map_fragment(struct SqshFileIterator *iterator) {
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
sqsh_file_iterator_next(
		struct SqshFileIterator *iterator, size_t desired_size, int *err) {
	int rv = 0;
	const struct SqshFile *file = iterator->file;
	size_t block_count = sqsh_file_block_count(file);
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

	if (iterator->sparse_size > 0) {
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
	return has_next;
}

int
sqsh_file_iterator_skip(
		struct SqshFileIterator *iterator, sqsh_index_t *offset,
		size_t desired_size) {
	int rv = 0;
	const size_t block_size = iterator->block_size;
	const sqsh_index_t skip_index = *offset / block_size;
	*offset = *offset % block_size;

	if (skip_index == 0 && iterator->block_index != 0) {
		goto out;
	}

	sqsh_index_t reader_forward = 0;
	sqsh_index_t block_index = iterator->block_index;
	for (sqsh_index_t i = 0; i < skip_index; i++) {
		reader_forward += sqsh_file_block_size(iterator->file, block_index);
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
	size_t current_size = sqsh_file_iterator_size(iterator);
	while (current_size <= *offset) {
		*offset -= current_size;
		bool has_next = sqsh_file_iterator_next(iterator, desired_size, &rv);
		if (rv < 0) {
			goto out;
		} else if (!has_next) {
			rv = -SQSH_ERROR_OUT_OF_BOUNDS;
			goto out;
		}
		current_size = sqsh_file_iterator_size(iterator);
	}

	rv = 0;

out:
	return rv;
}

const uint8_t *
sqsh_file_iterator_data(const struct SqshFileIterator *iterator) {
	return iterator->data;
}

size_t
sqsh_file_iterator_block_size(const struct SqshFileIterator *iterator) {
	return iterator->block_size;
}

size_t
sqsh_file_iterator_size(const struct SqshFileIterator *iterator) {
	return iterator->size;
}

int
sqsh__file_iterator_cleanup(struct SqshFileIterator *iterator) {
	sqsh__map_reader_cleanup(&iterator->map_reader);
	sqsh__extract_view_cleanup(&iterator->extract_view);
	sqsh__fragment_view_cleanup(&iterator->fragment_view);
	return 0;
}

int
sqsh_file_iterator_free(struct SqshFileIterator *iterator) {
	if (iterator == NULL) {
		return 0;
	}
	int rv = sqsh__file_iterator_cleanup(iterator);
	free(iterator);
	return rv;
}
