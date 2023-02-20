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
 * @file         sqsh_compression_private.h
 */

#ifndef SQSH_COMPRESSION_PRIVATE_H
#define SQSH_COMPRESSION_PRIVATE_H

#include "sqsh_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct SqshBuffer;

typedef int (*sqsh_extract_func_t)(
		uint8_t *target, size_t *target_size, const uint8_t *compressed,
		const size_t compressed_size);

struct SqshCompression {
	/**
	 * @privatesection
	 */
	sqsh_extract_func_t impl;
	size_t block_size;
};

/**
 * @internal
 * @memberof SqshCompression
 * @brief Initializes a compression context.
 *
 * @param[out] compression      The context to initialize.
 * @param[in]  compression_id   The id of the compression algorithm to use.
 * @param[in]  block_size       The block size to use for the compression.
 *
 * @return 0 on success, a negative value on error.
 */
int sqsh__compression_init(
		struct SqshCompression *compression, int compression_id,
		size_t block_size);

/**
 * @internal
 * @memberof SqshCompression
 * @brief Decompresses data to a buffer.
 *
 * @param[in]     compression     The compression context to use.
 * @param[out]    buffer          The buffer to store the decompressed data.
 * @param[in]     compressed      The compressed data to decompress.
 * @param[in]     compressed_size The size of the compressed data.
 *
 * @return 0 on success, a negative value on error.
 */
int sqsh__compression_decompress_to_buffer(
		const struct SqshCompression *compression, struct SqshBuffer *buffer,
		const uint8_t *compressed, const size_t compressed_size);

/**
 * @internal
 * @memberof SqshCompression
 * @brief Cleans up a compression context.
 *
 * @param[in] compression The context to clean up.
 *
 * @return 0 on success, a negative value on error.
 */
int sqsh__compression_cleanup(struct SqshCompression *compression);

#ifdef __cplusplus
}
#endif
#endif // SQSH_COMPRESSION_PRIVATE_H
