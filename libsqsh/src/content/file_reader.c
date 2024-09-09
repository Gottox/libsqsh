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
 * @file         file_reader.c
 */

#include <sqsh_content_private.h>

#include <sqsh_archive_private.h>
#include <sqsh_error.h>
#include <sqsh_reader_private.h>

#include <sqsh_common_private.h>

static bool
file_iterator_next(void *iterator, size_t desired_size, int *err) {
	return sqsh_file_iterator_next(iterator, desired_size, err);
}
static int
file_iterator_skip(void *iterator, uint64_t *offset, size_t desired_size) {
	return sqsh_file_iterator_skip2(iterator, offset, desired_size);
}
static const uint8_t *
file_iterator_data(const void *iterator) {
	return sqsh_file_iterator_data(iterator);
}
static size_t
file_iterator_size(const void *iterator) {
	return sqsh_file_iterator_size(iterator);
}

static const struct SqshReaderIteratorImpl file_reader_impl = {
		.next = file_iterator_next,
		.skip = file_iterator_skip,
		.data = file_iterator_data,
		.size = file_iterator_size,
};

int
sqsh__file_reader_init(
		struct SqshFileReader *reader, const struct SqshFile *file) {
	int rv = 0;
	rv = sqsh__file_iterator_init(&reader->iterator, file);
	if (rv < 0) {
		goto out;
	}
	rv = sqsh__reader_init(
			&reader->reader, &file_reader_impl, &reader->iterator);
out:
	return rv;
}

struct SqshFileReader *
sqsh_file_reader_new(const struct SqshFile *file, int *err) {
	SQSH_NEW_IMPL(sqsh__file_reader_init, struct SqshFileReader, file);
}

int
sqsh_file_reader_advance(
		struct SqshFileReader *reader, sqsh_index_t offset, size_t size) {
	return sqsh__reader_advance(&reader->reader, offset, size);
}

int
sqsh_file_reader_advance2(
		struct SqshFileReader *reader, uint64_t offset, size_t size) {
	return sqsh__reader_advance(&reader->reader, offset, size);
}

const uint8_t *
sqsh_file_reader_data(const struct SqshFileReader *reader) {
	return sqsh__reader_data(&reader->reader);
}

size_t
sqsh_file_reader_size(const struct SqshFileReader *reader) {
	return sqsh__reader_size(&reader->reader);
}

int
sqsh__file_reader_cleanup(struct SqshFileReader *reader) {
	sqsh__reader_cleanup(&reader->reader);
	sqsh__file_iterator_cleanup(&reader->iterator);

	return 0;
}

int
sqsh_file_reader_free(struct SqshFileReader *context) {
	SQSH_FREE_IMPL(sqsh__file_reader_cleanup, context);
}
