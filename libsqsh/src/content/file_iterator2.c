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
 * @file         file_iterator2.c
 */

#include <sqsh_common_private.h>
#include <sqsh_content_private.h>

#include <sqsh_archive_private.h>
#include <sqsh_error.h>
#include <sqsh_file.h>
#include <sqsh_file_private.h>

#define BLOCK_INDEX_FINISHED UINT32_MAX

int
sqsh__file_iterator2_init(
		struct SqshFileIterator2 *iterator, const struct SqshFile *file) {
	iterator->state = SQSH_FILE_ITERATOR_STATE_BLOCKS;
	iterator->file = file;
	return sqsh__block_iterator_init(&iterator->inner.block_iterator, file);
}

struct SqshFileIterator2 *
sqsh_file_iterator2_new(const struct SqshFile *file, int *err) {
	SQSH_NEW_IMPL(sqsh__file_iterator2_init, struct SqshFileIterator2, file);
}

int
sqsh__file_iterator2_copy(
		struct SqshFileIterator2 *target,
		const struct SqshFileIterator2 *source) {
	int rv = 0;

	target->state = source->state;
	switch (source->state) {
	case SQSH_FILE_ITERATOR_STATE_BLOCKS:
		rv = sqsh__block_iterator_copy(
				&target->inner.block_iterator, &source->inner.block_iterator);
		break;
	case SQSH_FILE_ITERATOR_STATE_FRAGMENT:
		rv = sqsh__fragment_view_copy(
				&target->inner.fragment_view, &source->inner.fragment_view);
		break;
	case SQSH_FILE_ITERATOR_STATE_DONE:
		rv = 0;
		break;
	}
	return rv;
}

bool
sqsh_file_iterator2_is_zero_block(const struct SqshFileIterator2 *iterator) {
	if (iterator->state == SQSH_FILE_ITERATOR_STATE_BLOCKS) {
		return sqsh__block_iterator_is_zero_block(
				&iterator->inner.block_iterator);
	} else {
		return false;
	}
}

bool
sqsh_file_iterator2_next(
		struct SqshFileIterator2 *iterator, size_t desired_size, int *err) {
	int rv = 0;
	bool has_next = false;
	switch (iterator->state) {
	case SQSH_FILE_ITERATOR_STATE_BLOCKS:
		has_next = sqsh__block_iterator_next(
				&iterator->inner.block_iterator, desired_size, err);
		if (has_next == false && sqsh_file_has_fragment(iterator->file)) {
			iterator->state = SQSH_FILE_ITERATOR_STATE_FRAGMENT;
			rv = sqsh__fragment_view_init(
					&iterator->inner.fragment_view, iterator->file);
			if (rv < 0) {
				goto out;
			}
			has_next = true;
		}
		break;
	case SQSH_FILE_ITERATOR_STATE_FRAGMENT:
		sqsh__fragment_view_cleanup(&iterator->inner.fragment_view);
		break;
	default:
		break;
	}
out:
	if (err != NULL) {
		*err = rv;
	}
	return has_next;
}

int
sqsh_file_iterator2_skip(
		struct SqshFileIterator2 *iterator, sqsh_index_t *offset,
		size_t desired_size) {
	int rv = 0;
	uint64_t offset64 = *offset;
	rv = sqsh_file_iterator2_skip2(iterator, &offset64, desired_size);
	*offset = offset64;
	return rv;
}

int
sqsh_file_iterator2_skip2(
		struct SqshFileIterator2 *iterator, uint64_t *offset,
		size_t desired_size) {
	(void)iterator;
	(void)offset;
	(void)desired_size;
	return 0;
}

const uint8_t *
sqsh_file_iterator2_data(const struct SqshFileIterator2 *iterator) {
	switch (iterator->state) {
	case SQSH_FILE_ITERATOR_STATE_BLOCKS:
		return sqsh__block_iterator_data(&iterator->inner.block_iterator);
	case SQSH_FILE_ITERATOR_STATE_FRAGMENT:
		return sqsh__fragment_view_data(&iterator->inner.fragment_view);
	default:
		return NULL;
	}
}

size_t
sqsh_file_iterator2_size(const struct SqshFileIterator2 *iterator) {
	switch (iterator->state) {
	case SQSH_FILE_ITERATOR_STATE_BLOCKS:
		return sqsh__block_iterator_size(&iterator->inner.block_iterator);
	case SQSH_FILE_ITERATOR_STATE_FRAGMENT:
		return sqsh__fragment_view_size(&iterator->inner.fragment_view);
	default:
		return 0;
	}
}

size_t
sqsh_file_iterator2_block_size(const struct SqshFileIterator2 *iterator) {
	(void)iterator;
	return SIZE_MAX;
}

int
sqsh__file_iterator2_cleanup(struct SqshFileIterator2 *iterator) {
	switch (iterator->state) {
	case SQSH_FILE_ITERATOR_STATE_BLOCKS:
		return sqsh__block_iterator_cleanup(&iterator->inner.block_iterator);
	case SQSH_FILE_ITERATOR_STATE_FRAGMENT:
		return sqsh__fragment_view_cleanup(&iterator->inner.fragment_view);
	default:
		return 0;
	}
}

int
sqsh_file_iterator2_free(struct SqshFileIterator2 *iterator) {
	SQSH_FREE_IMPL(sqsh__file_iterator2_cleanup, iterator);
}
