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
 * @file         sqsh_extract_private.h
 */

#ifndef SQSH_EXTRACT_PRIVATE_H
#define SQSH_EXTRACT_PRIVATE_H

#include <cextras/collection.h>

#include <sqsh_data.h>
#include <sqsh_utils_private.h>

#ifdef __cplusplus
extern "C" {
#endif

struct SqshArchive;
struct SqshMapReader;

/***************************************
 * extract/extractor2.c
 */

/**
 * @internal
 * @brief buffer type used by the extractor implementations.
 */
typedef uint8_t sqsh__extractor_context_t[256];

/**
 * @brief The implementation of the lzo extractor. This is NULL by default.
 * If you want to use this, you need to link against
 * [libsqsh-lzo](https://github.com/Gottox/libsqsh-lzo).
 */
extern const struct SqshExtractorImpl *const volatile sqsh__impl_lzo;

/**
 * @internal
 * @brief The SqshExtractorImpl struct is used to implement a extractor
 * that is then used by the SqshExtractor.
 */
struct SqshExtractorImpl {
	/**
	 * @brief Function that is called to initialize the extractor context.
	 */
	int (*init)(void *context, uint8_t *target, size_t target_size);
	/**
	 * @brief Function that is called when new data is available.
	 */
	int (*write)(
			void *context, const uint8_t *compressed,
			const size_t compressed_size);
	/**
	 * @brief Function that is called to finish the extraction.
	 */
	int (*finish)(void *context, uint8_t *target, size_t *target_size);
};

/**
 * @brief The SqshExtractor struct is used to decompress data.
 */
struct SqshExtractor {
	/**
	 * @privatesection
	 */
	struct CxBuffer *buffer;
	const struct SqshExtractorImpl *impl;
	sqsh__extractor_context_t context;
	uint8_t *target;
	size_t block_size;
};

/**
 * @internal
 * @memberof SqshExtractor
 * @brief Returns the extractor implementation for a given id.
 *
 * @param[in]  id   The id of the compression algorithm to use.
 *
 * @return pointer to the extractor implementation or NULL if the extraction
 * algorithm is not supported.
 */
SQSH_NO_EXPORT const struct SqshExtractorImpl *
sqsh__extractor_impl_from_id(enum SqshSuperblockCompressionId id);

/**
 * @internal
 * @memberof SqshExtractor
 * @brief Initializes a extractor context.
 *
 * @param[out] extractor      The context to initialize.
 * @param[out] buffer         The buffer to store the decompressed data.
 * @param[in]  impl           The implementation of the extraction algorithm.
 * @param[in]  block_size     The block size to use for the extraction.
 *
 * @return 0 on success, a negative value on error.
 */
SQSH_NO_EXPORT SQSH_NO_UNUSED int sqsh__extractor_init(
		struct SqshExtractor *extractor, struct CxBuffer *buffer,
		const struct SqshExtractorImpl *impl, size_t block_size);

/**
 * @internal
 * @memberof SqshExtractor
 * @brief Decompresses data to a buffer.
 *
 * @param[in]     extractor       The extractor context to use.
 * @param[in]     compressed      The compressed data to decompress.
 * @param[in]     compressed_size The size of the compressed data.
 *
 * @return 0 on success, a negative value on error.
 */
SQSH_NO_EXPORT SQSH_NO_UNUSED int sqsh__extractor_write(
		struct SqshExtractor *extractor, const uint8_t *compressed,
		const size_t compressed_size);

/**
 * @internal
 * @memberof SqshExtractor
 * @brief Cleans up a extractor context.
 *
 * @param[in] extractor The context to clean up.
 *
 * @return 0 on success, a negative value on error.
 */
SQSH_NO_EXPORT int sqsh__extractor_finish(struct SqshExtractor *extractor);

/**
 * @internal
 * @memberof SqshExtractor
 * @brief Cleans up a extractor context.
 *
 * @param[in] extractor The context to clean up.
 *
 * @return 0 on success, a negative value on error.
 */
SQSH_NO_EXPORT int sqsh__extractor_cleanup(struct SqshExtractor *extractor);

/***************************************
 * extract/extract_manager.c
 */

/**
 * @brief Manages chunks of compressed areas from an archive.
 */
struct SqshExtractManager {
	/**
	 * @privatesection
	 */
	struct CxRcRadixTree radix_tree;
	const struct SqshExtractorImpl *extractor_impl;
	uint32_t block_size;
	struct SqshMapManager *map_manager;
	struct CxLru lru;
	sqsh__mutex_t lock;
};

/**
 * @internal
 * @brief A buffer that is used by a SqshExtractManager.
 */
struct SqshExtractBuffer {
	uint64_t key;
	struct CxBuffer buffer;
};

/**
 * @internal
 * @memberof SqshExtractManager
 * @brief Initializes a extractor manager.
 *
 * @param[in]     manager     The manager to initialize.
 * @param[in]     archive     The archive to use.
 * @param[in]     block_size  The block size to use.
 * @param[in]     size        The size of the manager.
 *
 * @return 0 on success, a negative value on error.
 */
SQSH_NO_EXPORT SQSH_NO_UNUSED int sqsh__extract_manager_init(
		struct SqshExtractManager *manager, struct SqshArchive *archive,
		uint32_t block_size, size_t size);

/**
 * @internal
 * @memberof SqshExtractManager
 * @brief Decompresses data to a buffer.
 *
 * @param[in]     manager     The manager to use.
 * @param[in]     reader      The reader to use.
 * @param[out]    target      The buffer to store the decompressed data.
 *
 * @return 0 on success, a negative value on error.
 */
SQSH_NO_EXPORT int sqsh__extract_manager_uncompress(
		struct SqshExtractManager *manager, const struct SqshMapReader *reader,
		const struct SqshExtractBuffer **target);

/**
 * @internal
 * @memberof SqshExtractManager
 * @brief releases a buffer retrieved by sqsh__extract_manager_uncompress.
 *
 * @param[in] manager The manager to use.
 * @param[in] buffer  The buffer to release.
 *
 * @return 0 on success, a negative value on error.
 */
SQSH_NO_EXPORT int sqsh__extract_manager_release(
		struct SqshExtractManager *manager,
		const struct SqshExtractBuffer *buffer);

/**
 * @internal
 * @memberof SqshExtractManager
 * @brief Cleans up a extractor manager.
 *
 * @param[in] manager The manager to clean up.
 *
 * @return 0 on success, a negative value on error.
 */
SQSH_NO_EXPORT int
sqsh__extract_manager_cleanup(struct SqshExtractManager *manager);

/***************************************
 * extract/extract_view.c
 */

/**
 * @brief A fiew into compressed memory managed by a SqshExtractManager.
 */
struct SqshExtractView {
	/**
	 * @privatesection
	 */
	struct SqshExtractManager *manager;
	const struct SqshExtractBuffer *buffer;
	size_t offset;
	size_t size;
};

/**
 * @internal
 * @memberof SqshExtractView
 * @brief Initializes a extractor view.
 *
 * @param[in]     view        The view to initialize.
 * @param[in]     manager     The manager to use.
 * @param[in]     reader      The reader to use.
 *
 * @return 0 on success, a negative value on error.
 */
SQSH_NO_EXPORT SQSH_NO_UNUSED int sqsh__extract_view_init(
		struct SqshExtractView *view, struct SqshExtractManager *manager,
		const struct SqshMapReader *reader);

/**
 * @internal
 * @memberof SqshExtractView
 * @brief Gets the data of the view.
 *
 * @param[in]     view        The view to narrow.
 *
 * @return the data of the view.
 */
SQSH_NO_EXPORT const uint8_t *
sqsh__extract_view_data(const struct SqshExtractView *view);

/**
 * @internal
 * @memberof SqshExtractView
 * @brief Gets the size of the view.
 *
 * @param[in] view The view to get the size of.
 *
 * @return the size of the view.
 */
SQSH_NO_EXPORT size_t
sqsh__extract_view_size(const struct SqshExtractView *view);

/**
 * @internal
 * @memberof SqshExtractView
 * @brief Cleans up a extractor view.
 *
 * @param[in] view The view to clean up.
 *
 * @return 0 on success, a negative value on error.
 */
SQSH_NO_EXPORT int sqsh__extract_view_cleanup(struct SqshExtractView *view);

/***************************************
 * extract/lz4.c
 */

/**
 * @brief The implementation of the lz4 extractor.
 */
SQSH_NO_EXPORT extern const struct SqshExtractorImpl *const sqsh__impl_lz4;
/***************************************
 * extract/lzma.c
 */

/**
 * @brief The implementation of the lzma extractor.
 */
SQSH_NO_EXPORT extern const struct SqshExtractorImpl *const sqsh__impl_lzma;
/**
 * @brief The implementation of the xz extractor.
 */
SQSH_NO_EXPORT extern const struct SqshExtractorImpl *const sqsh__impl_xz;

/***************************************
 * extract/zlib.c
 */

/**
 * @brief The implementation of the zlib extractor.
 */
SQSH_NO_EXPORT extern const struct SqshExtractorImpl *const sqsh__impl_zlib;

/***************************************
 * extract/zstd.c
 */

/**
 * @brief The implementation of the zstd extractor.
 */
SQSH_NO_EXPORT extern const struct SqshExtractorImpl *const sqsh__impl_zstd;

#ifdef __cplusplus
}
#endif
#endif /* SQSH_EXTRACT_PRIVATE_H */
