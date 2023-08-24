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
 * @file         lz4.c
 */

#include "../../include/sqsh_extract_private.h"

#include "../../include/sqsh_error.h"

#ifdef CONFIG_LZ4

#	include <lz4.h>
#	include <cextras/collection.h>

SQSH_STATIC_ASSERT(
		sizeof(sqsh__extractor_context_t) >= sizeof(LZ4_streamDecode_t));

static int
sqsh_lz4_init(void *context, uint8_t *target, size_t target_size) {
	(void)target;
	(void)target_size;
	int rv = cx_buffer_init(context);
	if (rv < 0) {
		goto out;
	}
	rv = cx_buffer_add_capacity(context, NULL, target_size);
	if (rv < 0) {
		goto out;
	}
out:
	return rv;
}

static int
sqsh_lz4_decompress(
		void *context, const uint8_t *compressed,
		const size_t compressed_size) {
	return cx_buffer_append(context, compressed, compressed_size);
}

static int
sqsh_lz4_finish(void *context, uint8_t *target, size_t *target_size) {
	int rv = 0;
	const uint8_t *data = cx_buffer_data(context);
	size_t data_size = cx_buffer_size(context);

	int lz4_ret = LZ4_decompress_safe(
			(const char *)data, (char *)target, data_size, *target_size);
	if (lz4_ret < 0) {
		rv = -SQSH_ERROR_COMPRESSION_DECOMPRESS;
		goto out;
	}
	*target_size = lz4_ret;

out:
	cx_buffer_cleanup(context);
	return rv;
}

static const struct SqshExtractorImpl impl_lz4 = {
		.init = sqsh_lz4_init,
		.extract = sqsh_lz4_decompress,
		.finish = sqsh_lz4_finish,
};

const struct SqshExtractorImpl *const sqsh__impl_lz4 = &impl_lz4;
#else
const struct SqshExtractorImpl *const sqsh__impl_lz4 = NULL;
#endif
