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

#include "sqsh_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct SqshFile;
struct SqshArchive;

/***************************************
 * content/file_reader.c
 */

/**
 * @brief The file reader allows to read user defined byte ranges from a file
 * inside of a SqshArchive.
 */
struct SqshFileReader;

/**
 * @brief Initializes a SqshFileReader struct.
 * @memberof SqshFileReader
 *
 * @param[in] file The file context to retrieve the file contents from.
 * @param[out] err Pointer to an int where the error code will be stored.
 *
 * @return a new file reader.
 */
struct SqshFileReader *
sqsh_file_reader_new(const struct SqshFile *file, int *err);

/**
 * @deprecated Since 1.5.0. Use sqsh_file_reader_advance2() instead.
 * @brief Advances the file reader by a certain amount of data and presents
 * `size` bytes of data to the user.
 * @memberof SqshFileReader
 *
 * @param[in,out] reader The file reader to skip data in.
 * @param[in] offset The offset to skip.
 * @param[in] size The size of the data to skip.
 *
 * @return 0 on success, less than 0 on error.
 */
__attribute__((deprecated(
		"Since 1.5.0. Use sqsh_file_reader_advance2() instead."))) int
sqsh_file_reader_advance(
		struct SqshFileReader *reader, sqsh_index_t offset, size_t size);

/**
 * @brief Advances the file reader by a certain amount of data and presents
 * `size` bytes of data to the user.
 * @memberof SqshFileReader
 *
 * @param[in,out] reader The file reader to skip data in.
 * @param[in] offset The offset to skip.
 * @param[in] size The size of the data to skip.
 *
 * @return 0 on success, less than 0 on error.
 */
int sqsh_file_reader_advance2(
		struct SqshFileReader *reader, uint64_t offset, size_t size);

/**
 * @brief Gets a pointer to the current data in the file reader.
 * @memberof SqshFileReader
 *
 * @param[in] reader The file reader to get data from.
 *
 * @return A pointer to the current data in the file reader.
 */
const uint8_t *sqsh_file_reader_data(const struct SqshFileReader *reader);

/**
 * @brief Gets the size of the current data in the file reader.
 * @memberof SqshFileReader
 *
 * @param[in] reader The file reader to get data from.
 *
 * @return The size of the current data in the file reader.
 */
size_t sqsh_file_reader_size(const struct SqshFileReader *reader);

/**
 * @brief Cleans up resources used by a SqshFileReader struct.
 * @memberof SqshFileReader
 *
 * @param[in,out] reader The file reader struct to clean up.
 *
 * @return 0 on success, less than 0 on error.
 */
int sqsh_file_reader_free(struct SqshFileReader *reader);

/***************************************
 * content/file_iterator.c
 */

struct SqshFileIterator;

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
SQSH_NO_UNUSED struct SqshFileIterator *
sqsh_file_iterator_new(const struct SqshFile *file, int *err);

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
SQSH_NO_UNUSED bool sqsh_file_iterator_next(
		struct SqshFileIterator *iterator, size_t desired_size, int *err);

/**
 * @brief Checks if the current block is a zero block.
 * @memberof SqshFileIterator
 *
 * @param[in] iterator The file iterator to check.
 *
 * @return true if the current block is a zero block, false otherwise.
 */
SQSH_NO_UNUSED bool
sqsh_file_iterator_is_zero_block(const struct SqshFileIterator *iterator);

/**
 * @deprecated Since 1.5.0. Use sqsh_file_iterator_skip2() instead.
 * @memberof SqshFileIterator
 * @brief Skips blocks until the block containing the offset is reached.
 * Note that calling this function will invalidate the data pointer returned by
 * sqsh_file_iterator_data().
 *
 * The offset is relative to the beginning of the current block or, if the
 * iterator hasn't been forwarded with previous calls to
 * sqsh_file_iterator_skip() or sqsh_file_iterator_next() the beginning of the
 * first block.
 *
 * After calling this function `offset` is updated to the same position relative
 * to the new block. See this visualisation:
 *
 * ```
 * current_block: |<--- block 8000 --->|
 *                           offset = 10000 --^
 * -->  sqsh_file_iterator_skip(i, &offset, 1)
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
		deprecated("Since 1.5.0. Use sqsh_file_iterator_skip2() instead.")))
SQSH_NO_UNUSED int
sqsh_file_iterator_skip(
		struct SqshFileIterator *iterator, sqsh_index_t *offset,
		size_t desired_size);

/**
 * @memberof SqshFileIterator
 * @brief Skips blocks until the block containing the offset is reached.
 * Note that calling this function will invalidate the data pointer returned by
 * sqsh_file_iterator_data().
 *
 * The offset is relative to the beginning of the current block or, if the
 * iterator hasn't been forwarded with previous calls to
 * sqsh_file_iterator_skip() or sqsh_file_iterator_next() the beginning of the
 * first block.
 *
 * After calling this function `offset` is updated to the same position relative
 * to the new block. See this visualisation:
 *
 * ```
 * current_block: |<--- block 8000 --->|
 *                           offset = 10000 --^
 * -->  sqsh_file_iterator_skip(i, &offset, 1)
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
SQSH_NO_UNUSED int sqsh_file_iterator_skip2(
		struct SqshFileIterator *iterator, uint64_t *offset,
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
sqsh_file_iterator_data(const struct SqshFileIterator *iterator);

/**
 * @brief Returns the block size of the file iterator.
 * @memberof SqshFileIterator
 *
 * @param[in] iterator The file iterator to get the size from.
 *
 * @return The size of the data currently in the file iterator.
 */
SQSH_NO_UNUSED size_t
sqsh_file_iterator_block_size(const struct SqshFileIterator *iterator);

/**
 * @brief Gets the size of the data currently in the file iterator.
 * @memberof SqshFileIterator
 *
 * @param[in] iterator The file iterator to get the size from.
 *
 * @return The size of the data currently in the file iterator.
 */
SQSH_NO_UNUSED size_t
sqsh_file_iterator_size(const struct SqshFileIterator *iterator);

/**
 * @brief Frees the resources used by a SqshFileIterator struct.
 * @memberof SqshFileIterator
 *
 * @param[in,out] iterator The file iterator to free.
 *
 * @return 0 on success, less than 0 on error.
 */
int sqsh_file_iterator_free(struct SqshFileIterator *iterator);

#ifdef __cplusplus
}
#endif
#endif /* SQSH_CONTENT_H */
