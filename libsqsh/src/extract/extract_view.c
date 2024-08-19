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
 * @file         extract_view.c
 */

#include <sqsh_common_private.h>
#include <sqsh_error.h>
#include <sqsh_extract_private.h>
#include <sqsh_mapper_private.h>

int
sqsh__extract_view_init(
		struct SqshExtractView *view, struct SqshExtractManager *manager,
		const struct SqshMapReader *reader) {
	int rv = 0;
	view->manager = manager;
	view->buffer = NULL;
	view->address = sqsh__map_reader_address(reader);

	rv = sqsh__extract_manager_uncompress(manager, reader, &view->buffer);
	if (rv < 0) {
		goto out;
	}

	view->size = cx_buffer_size(view->buffer);

out:
	if (rv < 0) {
		sqsh__extract_view_cleanup(view);
	}
	return rv;
}

int
sqsh__extract_view_copy(
		struct SqshExtractView *target, const struct SqshExtractView *source) {
	int rv = 0;

	target->manager = source->manager;
	if (source->buffer) {
		rv = sqsh__extract_manager_retain_buffer(
				source->manager, source->buffer);
		if (rv < 0) {
			goto out;
		}
	}
	target->buffer = source->buffer;
	target->size = source->size;
out:
	if (rv < 0) {
		sqsh__extract_view_cleanup(target);
	}
	return rv;
}

const uint8_t *
sqsh__extract_view_data(const struct SqshExtractView *view) {
	return cx_buffer_data(view->buffer);
}

size_t
sqsh__extract_view_size(const struct SqshExtractView *view) {
	return view->size;
}

int
sqsh__extract_view_cleanup(struct SqshExtractView *view) {
	int rv = 0;

	if (view->manager != NULL) {
		rv = sqsh__extract_manager_release(view->manager, view->address);
	}
	view->buffer = NULL;
	view->size = 0;
	return rv;
}
