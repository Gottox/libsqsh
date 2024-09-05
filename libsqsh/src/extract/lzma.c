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
 * @file         lzma.c
 */

#include <sqsh_extract_private.h>

#include <sqsh_error.h>

#ifdef CONFIG_LZMA

#	include <lzma.h>
#	include <string.h>

SQSH_STATIC_ASSERT(sizeof(sqsh__extractor_context_t) >= sizeof(lzma_stream));

static const lzma_stream proto_stream = LZMA_STREAM_INIT;

enum SqshLzmaType {
	LZMA_TYPE_ALONE,
	LZMA_TYPE_XZ,
};

static int
sqsh_lzma_init(
		void *context, uint8_t *target, size_t target_size,
		enum SqshLzmaType type) {
	lzma_stream *stream = context;
	memcpy(stream, &proto_stream, sizeof(lzma_stream));

	lzma_ret ret;
	if (type == LZMA_TYPE_ALONE) {
		ret = lzma_alone_decoder(stream, UINT64_MAX);
	} else {
		ret = lzma_stream_decoder(stream, UINT64_MAX, 0);
	}

	if (ret != LZMA_OK) {
		return -SQSH_ERROR_COMPRESSION_DECOMPRESS;
	}

	stream->next_out = target;
	stream->avail_out = target_size;

	return 0;
}

static int
sqsh_lzma_init_xz(void *context, uint8_t *target, size_t target_size) {
	return sqsh_lzma_init(context, target, target_size, LZMA_TYPE_XZ);
}

static int
sqsh_lzma_init_alone(void *context, uint8_t *target, size_t target_size) {
	return sqsh_lzma_init(context, target, target_size, LZMA_TYPE_ALONE);
}

static int
sqsh_lzma_decompress(
		void *context, const uint8_t *compressed,
		const size_t compressed_size) {
	lzma_stream *stream = context;

	stream->next_in = compressed;
	stream->avail_in = compressed_size;

	lzma_ret ret = lzma_code(stream, LZMA_RUN);
	if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
		return -SQSH_ERROR_COMPRESSION_DECOMPRESS;
	}

	return 0;
}

static int
sqsh_lzma_finish(void *context, uint8_t *target, size_t *target_size) {
	(void)target;
	lzma_stream *stream = context;
	stream->next_in = NULL;
	stream->avail_in = 0;

	lzma_ret ret = lzma_code(stream, LZMA_FINISH);

	*target_size = (size_t)stream->total_out;
	lzma_end(stream);

	if (ret == LZMA_STREAM_END) {
		return 0;
	} else {
		*target_size = 0;
		return -SQSH_ERROR_COMPRESSION_DECOMPRESS;
	}
}

static const struct SqshExtractorImpl impl_xz = {
		.init = sqsh_lzma_init_xz,
		.write = sqsh_lzma_decompress,
		.finish = sqsh_lzma_finish,
};

const struct SqshExtractorImpl *const sqsh__impl_xz = &impl_xz;

static const struct SqshExtractorImpl impl_lzma = {
		.init = sqsh_lzma_init_alone,
		.write = sqsh_lzma_decompress,
		.finish = sqsh_lzma_finish,
};

const struct SqshExtractorImpl *const sqsh__impl_lzma = &impl_lzma;
#else
const struct SqshExtractorImpl *const sqsh__impl_lzma = NULL;
const struct SqshExtractorImpl *const sqsh__impl_xz = NULL;
#endif
