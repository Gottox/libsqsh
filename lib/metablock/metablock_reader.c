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
 * @file         metablock_iterator.c
 */

#include "../../include/sqsh_compression_private.h"

#include "../../include/sqsh_metablock_private.h"
#include "../../include/sqsh_archive.h"

#include "../../include/sqsh_error.h"
#include "../utils.h"

static int
append_to_buffer(
		const struct SqshMetablockReader *reader, struct SqshBuffer *buffer) {
	int rv = 0;

	const struct SqshMetablockIterator *iterator = &reader->iterator;
	bool is_compressed = sqsh__metablock_iterator_is_compressed(iterator);

	if (!is_compressed) {
		const uint8_t *data = sqsh__metablock_iterator_data(iterator);
		size_t size = sqsh__metablock_iterator_size(iterator);
		rv = sqsh__buffer_append(buffer, data, size);
	} else if (reader->compression_manager == NULL) {
		const uint8_t *data = sqsh__metablock_iterator_data(iterator);
		size_t size = sqsh__metablock_iterator_size(iterator);

		return sqsh__compression_decompress_to_buffer(
				reader->compression, buffer, data, size);

	} else {
		const struct SqshBuffer *uncompressed = NULL;
		struct SqshCompressionManager *manager = reader->compression_manager;
		uint64_t address = sqsh__metablock_iterator_data_address(iterator);
		size_t metablock_size = sqsh__metablock_iterator_size(iterator);
		rv = sqsh__compression_manager_get(
				manager, address, metablock_size, &uncompressed);
		if (rv < 0) {
			return rv;
		}
		const uint8_t *data = sqsh__buffer_data(uncompressed);
		uint16_t size = sqsh__buffer_size(uncompressed);
		rv = sqsh__buffer_append(buffer, data, size);
		sqsh__compression_manager_release(manager, uncompressed);
	}
	return rv;
}

int
sqsh__metablock_reader_init(
		struct SqshMetablockReader *cursor, struct SqshArchive *sqsh,
		struct SqshCompressionManager *compression_manager,
		const uint64_t start_address, const uint64_t upper_limit) {
	int rv;
	rv = sqsh__metablock_iterator_init(
			&cursor->iterator, sqsh, start_address, upper_limit);
	if (rv < 0) {
		goto out;
	}
	rv = sqsh__buffer_init(&cursor->buffer);
	if (rv < 0) {
		goto out;
	}
	cursor->size = 0;
	cursor->offset = 0;
	cursor->compression = sqsh_archive_compression_metablock(sqsh);
	cursor->compression_manager = compression_manager;

out:
	if (rv < 0) {
		sqsh__metablock_reader_cleanup(cursor);
	}
	return rv;
}

int
sqsh__metablock_reader_advance(
		struct SqshMetablockReader *cursor, sqsh_index_t offset, size_t size) {
	int rv;
	sqsh_index_t new_offset;
	sqsh_index_t end_offset;
	size_t new_size;

	if (SQSH_ADD_OVERFLOW(offset, cursor->offset, &new_offset)) {
		return -SQSH_ERROR_INTEGER_OVERFLOW;
	}
	if (SQSH_ADD_OVERFLOW(new_offset, size, &new_size)) {
		return -SQSH_ERROR_INTEGER_OVERFLOW;
	}
	if (SQSH_ADD_OVERFLOW(new_offset, size, &end_offset)) {
		return -SQSH_ERROR_INTEGER_OVERFLOW;
	}

	while (sqsh__buffer_size(&cursor->buffer) < end_offset) {
		rv = sqsh__metablock_iterator_next(&cursor->iterator);
		if (rv < 0) {
			return rv;
		}

		rv = append_to_buffer(cursor, &cursor->buffer);
		if (rv < 0) {
			return rv;
		}
	}
	cursor->offset = new_offset;
	cursor->size = size;
	return 0;
}

const uint8_t *
sqsh__metablock_reader_data(const struct SqshMetablockReader *cursor) {
	const uint8_t *data = sqsh__buffer_data(&cursor->buffer);

	return &data[cursor->offset];
}

size_t
sqsh__metablock_reader_size(const struct SqshMetablockReader *cursor) {
	return cursor->size;
}

int
sqsh__metablock_reader_cleanup(struct SqshMetablockReader *cursor) {
	sqsh__metablock_iterator_cleanup(&cursor->iterator);
	sqsh__buffer_cleanup(&cursor->buffer);

	return 0;
}
