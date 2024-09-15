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

#include <sqsh_content_private.h>

#include <cextras/collection.h>
#include <sqsh_archive_private.h>
#include <sqsh_common_private.h>
#include <sqsh_error.h>
#include <sqsh_file_private.h>
#include <stdint.h>

int
sqsh__file_iterator_init(
		struct SqshFileIterator *iterator, const struct SqshFile *file) {
	return sqsh__block_iterator_init(&iterator->block_iterator, file);
}

int
sqsh__file_iterator_copy(
		struct SqshFileIterator *target,
		const struct SqshFileIterator *source) {
	return sqsh__block_iterator_copy(
			&target->block_iterator, &source->block_iterator);
}

struct SqshFileIterator *
sqsh_file_iterator_new(const struct SqshFile *file, int *err) {
	SQSH_NEW_IMPL(sqsh__file_iterator_init, struct SqshFileIterator, file);
}

bool
sqsh_file_iterator_is_zero_block(const struct SqshFileIterator *iterator) {
	return sqsh__block_iterator_is_zero_block(&iterator->block_iterator);
}

bool
sqsh_file_iterator_next(
		struct SqshFileIterator *iterator, size_t desired_size, int *err) {
	return sqsh__block_iterator_next(
			&iterator->block_iterator, desired_size, err);
}

int
sqsh_file_iterator_skip(
		struct SqshFileIterator *iterator, sqsh_index_t *offset,
		size_t desired_size) {
	uint64_t offset64 = *offset;
	int rv = sqsh_file_iterator_skip2(iterator, &offset64, desired_size);
	*offset = (size_t)offset64;
	return rv;
}

int
sqsh_file_iterator_skip2(
		struct SqshFileIterator *iterator, uint64_t *offset,
		size_t desired_size) {
	return sqsh__block_iterator_skip(
			&iterator->block_iterator, offset, desired_size);
}

const uint8_t *
sqsh_file_iterator_data(const struct SqshFileIterator *iterator) {
	return sqsh__block_iterator_data(&iterator->block_iterator);
}

size_t
sqsh_file_iterator_block_size(const struct SqshFileIterator *iterator) {
	return sqsh__block_iterator_block_size(&iterator->block_iterator);
}

size_t
sqsh_file_iterator_size(const struct SqshFileIterator *iterator) {
	return sqsh__block_iterator_size(&iterator->block_iterator);
}

int
sqsh__file_iterator_cleanup(struct SqshFileIterator *iterator) {
	return sqsh__block_iterator_cleanup(&iterator->block_iterator);
}

int
sqsh_file_iterator_free(struct SqshFileIterator *iterator) {
	SQSH_FREE_IMPL(sqsh__file_iterator_cleanup, iterator);
}
