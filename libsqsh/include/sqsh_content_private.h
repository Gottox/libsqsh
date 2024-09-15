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
 * @file         sqsh_content_private.h
 */

#ifndef SQSH_CONTENT_PRIVATE_H
#define SQSH_CONTENT_PRIVATE_H

#include <sqsh_content.h>

#include "sqsh_extract_private.h"
#include "sqsh_mapper_private.h"
#include "sqsh_reader_private.h"

#ifdef __cplusplus
extern "C" {
#endif

struct SqshFile;

/***************************************
 * content/fragment_view.c
 */

/**
 * @brief A view over the contents of a fragment.
 */
struct SqshFragmentView {
	/**
	 * @privatesection
	 */
	const struct SqshFragmentTable *fragment_table;
	struct SqshMapReader map_reader;
	struct SqshExtractView extract_view;
	const uint8_t *data;
	size_t size;
};

/**
 * @internal
 * @memberof SqshFragmentView
 * @brief Initializes a fragment view to access the fragment contents of a
single file.
 *
 * @param[in,out] view The fragment view to initialize.
 * @param[in] file The file to retrieve the fragment contents from.
 *
 * @return 0 on success, less than 0 on error.
 */
SQSH_NO_EXPORT int sqsh__fragment_view_init(
		struct SqshFragmentView *view, const struct SqshFile *file);

/**
 * @internal
 * @memberof SqshFragmentView
 * @brief Creates a copy of an fragment view.
 *
 * @param[out] target The view to copy to.
 * @param[in]  source The view to copy from.
 *
 * @return 0 on success, a negative value on error.
 */
SQSH_NO_EXPORT SQSH_NO_UNUSED int sqsh__fragment_view_copy(
		struct SqshFragmentView *target, const struct SqshFragmentView *source);

/**
 * @internal
 * @memberof SqshFragmentView
 * @brief Retrieves the fragment data.
 *
 * @param[in] view The fragment view to retrieve the data from.
 *
 * @return The fragment data.
 */
SQSH_NO_EXPORT const uint8_t *
sqsh__fragment_view_data(const struct SqshFragmentView *view);

/**
 * @internal
 * @memberof SqshFragmentView
 * @brief Retrieves the fragment size.
 *
 * @param[in] view The fragment view to retrieve the size from.
 *
 * @return The fragment size.
 */
SQSH_NO_EXPORT size_t
sqsh__fragment_view_size(const struct SqshFragmentView *view);

/**
 * @internal
 * @memberof SqshFragmentView
 * @brief Cleans up resources used by a fragment view.
 *
 * @param[in] view The fragment view to clean up.
 *
 * @return 0 on success, less than 0 on error.
 */
SQSH_NO_EXPORT int sqsh__fragment_view_cleanup(struct SqshFragmentView *view);

/***************************************
 * content/file_iterator.c
 */

/**
 * @brief An iterator over the contents of a file.
 */
struct SqshFileIterator {
	/**
	 * @privatesection
	 */
	const struct SqshFile *file;
	struct SqshExtractManager *compression_manager;
	struct SqshMapReader map_reader;
	struct SqshExtractView extract_view;
	struct SqshFragmentView fragment_view;
	size_t sparse_size;
	size_t block_size;
	uint64_t block_index;
	const uint8_t *data;
	size_t size;
};

/**
 * @internal
 * @memberof SqshFileIterator
 * @brief Initializes a file iterator to iterate over the contents of a file.
 *
 * @param[in,out] iterator The file iterator to initialize.
 * @param[in] file The file to iterate over.
 *
 * @return 0 on success, less than 0 on error.
 */
SQSH_NO_EXPORT SQSH_NO_UNUSED int sqsh__file_iterator_init(
		struct SqshFileIterator *iterator, const struct SqshFile *file);

/**
 * @internal
 * @memberof SqshFileIterator
 * @brief Creates a copy of a file iterator.
 *
 * @param[out] target The iterator to copy to.
 * @param[in] source The iterator to copy from.
 *
 * @return 0 on success, less than 0 on error.
 */
SQSH_NO_EXPORT SQSH_NO_UNUSED int sqsh__file_iterator_copy(
		struct SqshFileIterator *target, const struct SqshFileIterator *source);

/**
 * @internal
 * @memberof SqshFileIterator
 * @brief Cleans up resources used by a file iterator.
 *
 * @param[in] iterator The file iterator to clean up.
 *
 * @return 0 on success, less than 0 on error.
 */
SQSH_NO_EXPORT int
sqsh__file_iterator_cleanup(struct SqshFileIterator *iterator);

/***************************************
 * context/file_reader.c
 */

/**
 * @brief A reader over the contents of a file.
 */
struct SqshFileReader {
	/**
	 * @privatesection
	 */
	struct SqshFileIterator iterator;
	struct SqshReader reader;
};

/**
 * @internal
 * @memberof SqshFileReader
 * @brief Initializes a SqshFileReader struct.
 *
 * @param[out] reader The file reader to initialize.
 * @param[in] file    The file context to retrieve the contents from.
 *
 * @return 0 on success, less than 0 on error.
 */
SQSH_NO_EXPORT SQSH_NO_UNUSED int sqsh__file_reader_init(
		struct SqshFileReader *reader, const struct SqshFile *file);

/**
 * @internal
 * @memberof SqshFileReader
 * @brief Frees the resources used by the file reader.
 *
 * @param reader The file reader to clean up.
 *
 * @return 0 on success, less than 0 on error.
 */
SQSH_NO_EXPORT int sqsh__file_reader_cleanup(struct SqshFileReader *reader);

#ifdef __cplusplus
}
#endif
#endif /* SQSH_CONTENT_PRIVATE_H */
