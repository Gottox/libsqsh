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
 * @file         sqsh_file.h
 */

#ifndef SQSH_CONTENT_H
#define SQSH_CONTENT_H

#include <sqsh_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************
 * file/file_iterator.c
 */

struct SqshFileIterator2;
struct SqshFile;

/**
 * @brief Creates a new SqshFileIterator struct and initializes it.
 * @memberof SqshFileIterator
 *
 * @param[in] file The file context to retrieve the file contents from.
 * @param[out] err Pointer to an int where the error code will be stored.
 *
 * @return A pointer to the newly created and initialized SqshFileIterator
 * struct.
 */
SQSH_NO_UNUSED struct SqshFileIterator2 *
sqsh_file_iterator2_new(const struct SqshFile *file, int *err);

/**
 * @brief Reads a certain amount of data from the file iterator.
 * @memberof SqshFileIterator
 *
 * @param[in,out] iterator The file iterator to read data from.
 * @param[in] desired_size The desired size of the data to read. May be more or
 * less than the actual size of the data read.
 * @param[out] err Pointer to an int where the error code will be stored.
 *
 * @retval true  When the iterator was advanced
 * @retval false When the iterator is at the end and no more entries are
 * available or if an error occured.
 */
SQSH_NO_UNUSED bool sqsh_file_iterator2_next(
		struct SqshFileIterator2 *iterator, size_t desired_size, int *err);

/**
 * @brief Checks if the current block is a zero block.
 * @memberof SqshFileIterator
 *
 * @param[in] iterator The file iterator to check.
 *
 * @return true if the current block is a zero block, false otherwise.
 */
SQSH_NO_UNUSED bool
sqsh_file_iterator2_is_zero_block(const struct SqshFileIterator2 *iterator);

/**
 * @deprecated Since 1.5.0. Use sqsh_file_iterator2_skip2() instead.
 * @memberof SqshFileIterator
 * @brief Skips blocks until the block containing the offset is reached.
 * Note that calling this function will invalidate the data pointer returned by
 * sqsh_file_iterator2_data().
 *
 * The offset is relative to the beginning of the current block or, if the
 * iterator hasn't been forwarded with previous calls to
 * sqsh_file_iterator2_skip() or sqsh_file_iterator2_next() the beginning of the
 * first block.
 *
 * After calling this function `offset` is updated to the same position relative
 * to the new block. See this visualisation:
 *
 * ```
 * current_block: |<--- block 8000 --->|
 *                           offset = 10000 --^
 * -->  sqsh_file_iterator2_skip(i, &offset, 1)
 * current_block:                      |<--- block 8000 --->|
 *                            offset = 2000 --^
 * ```
 *
 * If libsqsh can map more than one block at once, it will do so until
 * `desired_size` is reached. Note that `desired_size` is only a hint and
 * libsqsh may return more or less data than requested.
 *
 * @param[in,out] iterator      The file iterator to skip data in.
 * @param[in,out] offset        The offset that is contained in the block to
 * skip to.
 * @param[in] desired_size      The desired size of the data to read.
 *
 * @return 0 on success, less than 0 on error.
 */
__attribute__((
		deprecated("Since 1.5.0. Use sqsh_file_iterator2_skip2() instead.")))
SQSH_NO_UNUSED int
sqsh_file_iterator2_skip(
		struct SqshFileIterator2 *iterator, sqsh_index_t *offset,
		size_t desired_size);

/**
 * @memberof SqshFileIterator
 * @brief Skips blocks until the block containing the offset is reached.
 * Note that calling this function will invalidate the data pointer returned by
 * sqsh_file_iterator2_data().
 *
 * The offset is relative to the beginning of the current block or, if the
 * iterator hasn't been forwarded with previous calls to
 * sqsh_file_iterator2_skip() or sqsh_file_iterator2_next() the beginning of the
 * first block.
 *
 * After calling this function `offset` is updated to the same position relative
 * to the new block. See this visualisation:
 *
 * ```
 * current_block: |<--- block 8000 --->|
 *                           offset = 10000 --^
 * -->  sqsh_file_iterator2_skip(i, &offset, 1)
 * current_block:                      |<--- block 8000 --->|
 *                            offset = 2000 --^
 * ```
 *
 * If libsqsh can map more than one block at once, it will do so until
 * `desired_size` is reached. Note that `desired_size` is only a hint and
 * libsqsh may return more or less data than requested.
 *
 * @param[in,out] iterator      The file iterator to skip data in.
 * @param[in,out] offset        The offset that is contained in the block to
 * skip to.
 * @param[in] desired_size      The desired size of the data to read.
 *
 * @return 0 on success, less than 0 on error.
 */
SQSH_NO_UNUSED int sqsh_file_iterator2_skip2(
		struct SqshFileIterator2 *iterator, uint64_t *offset,
		size_t desired_size);

/**
 * @brief Gets a pointer to the current data in the file iterator.
 * @memberof SqshFileIterator
 *
 * @param[in] iterator The file iterator to get data from.
 *
 * @return A pointer to the current data in the file iterator.
 */
SQSH_NO_UNUSED const uint8_t *
sqsh_file_iterator2_data(const struct SqshFileIterator2 *iterator);

/**
 * @brief Returns the block size of the file iterator.
 * @memberof SqshFileIterator
 *
 * @param[in] iterator The file iterator to get the size from.
 *
 * @return The size of the data currently in the file iterator.
 */
SQSH_NO_UNUSED size_t
sqsh_file_iterator2_block_size(const struct SqshFileIterator2 *iterator);

/**
 * @brief Gets the size of the data currently in the file iterator.
 * @memberof SqshFileIterator
 *
 * @param[in] iterator The file iterator to get the size from.
 *
 * @return The size of the data currently in the file iterator.
 */
SQSH_NO_UNUSED size_t
sqsh_file_iterator2_size(const struct SqshFileIterator2 *iterator);

/**
 * @brief Frees the resources used by a SqshFileIterator struct.
 * @memberof SqshFileIterator
 *
 * @param[in,out] iterator The file iterator to free.
 *
 * @return 0 on success, less than 0 on error.
 */
int sqsh_file_iterator2_free(struct SqshFileIterator2 *iterator);

#ifdef __cplusplus
}
#endif
#endif /* SQSH_CONTENT_H */
