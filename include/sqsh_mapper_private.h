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
 * @file         sqsh_mapper_private.h
 */

#ifndef SQSH_PRIVATE_MAPPER_H
#define SQSH_PRIVATE_MAPPER_H

#include "sqsh_mapper.h"

#include "sqsh_reader_private.h"
#include "sqsh_thread_private.h"
#include <cextras/collection.h>
#include <sys/wait.h>

#ifdef __cplusplus
extern "C" {
#endif

struct SqshArchive;
struct SqshConfig;

/***************************************
 * mapper/curl_mapper.c
 */

/**
 * @brief The curl mapper.
 */
struct SqshCurlMapper {
	/**
	 * @privatesection
	 */
	char *url;
	uint64_t expected_time;
	void *handle;
	uint8_t *header_cache;
	sqsh_index_t archive_offset;
	sqsh__mutex_t lock;
};

/***************************************
 * mapper/mmap_mapper.c
 */

/**
 * @brief The mmap mapper.
 */
struct SqshMmapMapper {
	/**
	 * @privatesection
	 */
	int fd;
	long page_size;
};

/***************************************
 * mapper/static_mapper.c
 */

/**
 * @brief The static mapper.
 */
struct SqshStaticMapper {
	/**
	 * @privatesection
	 */
	const uint8_t *data;
};

/***************************************
 * mapper/mapper.c
 */

struct SqshMapSlice;

/**
 * @brief The mapper that is used to map chunks of the archive into memory.
 */
struct SqshMapper {
	/**
	 * @privatesection
	 */
	const struct SqshMemoryMapperImpl *impl;
	size_t block_size;
	size_t archive_size;
	union {
		struct SqshMmapMapper mm;
		struct SqshStaticMapper sm;
		struct SqshCurlMapper cl;
	} data;
};

/**
 * @brief The implementation of a memory mapper.
 */
struct SqshMemoryMapperImpl {
	/**
	 * @privatesection
	 */
	size_t block_size_hint;
	int (*init)(
			struct SqshMapper *mapper, const void *input, size_t *size,
			const struct SqshConfig *config);
	int (*map)(struct SqshMapSlice *map);
	const uint8_t *(*map_data)(const struct SqshMapSlice *mapping);
	int (*unmap)(struct SqshMapSlice *mapping);
	int (*cleanup)(struct SqshMapper *mapper);
};

/**
 * @internal
 * @memberof SqshMapper
 * @brief Initializes a mapper with an implementation and input data.
 *
 * @param[out] mapper The mapper to initialize.
 * @param[in]  source The input data to map.
 * @param[in]  config The configuration of this sqsh archive.
 *
 * @return 0 on success, a negative value on error.
 */
SQSH_NO_EXPORT SQSH_NO_UNUSED int sqsh__mapper_init(
		struct SqshMapper *mapper, const void *source,
		const struct SqshConfig *config);

/**
 * @internal
 * @memberof SqshMapper
 * @brief Retrieves the size of the input data in a mapper.
 *
 * @param[in] mapper The mapper to retrieve the size from.
 *
 * @return The size of the input data in the mapper.
 */
SQSH_NO_EXPORT size_t sqsh__mapper_size(const struct SqshMapper *mapper);

/**
 * @internal
 * @memberof SqshMapper
 * @brief Retrieves the block size for a mapper.
 *
 * @param[in] mapper The mapper to retrieve the size from.
 *
 * @return The size of the input data in the mapper.
 */
SQSH_NO_EXPORT size_t sqsh__mapper_block_size(const struct SqshMapper *mapper);

/**
 * @internal
 * @memberof SqshMapper
 * @brief Cleans up a mapper.
 *
 * @param[in] mapper The mapper to clean up.
 *
 * @return 0 on success, a negative value on error.
 */
SQSH_NO_EXPORT int sqsh__mapper_cleanup(struct SqshMapper *mapper);

/***************************************
 * mapper/map_slice.c
 */

/**
 * @brief Represents a view into the data of an archive.
 */
struct SqshMapSlice {
	/**
	 * @privatesection
	 */
	struct SqshMapper *mapper;
	sqsh_index_t offset;
	size_t size;
	void *data;
};

/**
 * @internal
 * @memberof SqshMapSlice
 * @brief Retrieves the data in a mapping.
 *
 * @param[in] mapping The mapping to retrieve the data from.
 *
 * @return The data in the mapping.
 */
SQSH_NO_EXPORT const uint8_t *
sqsh__map_slice_data(const struct SqshMapSlice *mapping);

/**
 * @internal
 * @memberof SqshMapSlice
 * @brief Retrieves the data in a mapping.
 *
 * @param[in] mapping The mapping to retrieve the data from.
 *
 * @return The data in the mapping.
 */
SQSH_NO_EXPORT size_t sqsh__map_slice_size(const struct SqshMapSlice *mapping);

/**
 * @internal
 * @memberof SqshMapSlice
 * @brief Unmaps a mapping.
 *
 * @param[in] mapping The mapping to unmap.
 *
 * @return 0 on success, a negative value on error.
 */
SQSH_NO_EXPORT int sqsh__map_slice_cleanup(struct SqshMapSlice *mapping);
/**
 * @internal
 * @memberof SqshMapper
 * @brief Maps a portion of the input data to a mapping.
 *
 * @param[out] mapping The mapping to store the mapped data.
 * @param[in]  mapper The mapper to use for the mapping.
 * @param[in]  offset The offset in the input data to start the mapping.
 * @param[in]  size The size of the mapped data.
 *
 * @return 0 on success, a negative value on error.
 */
SQSH_NO_EXPORT SQSH_NO_UNUSED int sqsh__map_slice_init(
		struct SqshMapSlice *mapping, struct SqshMapper *mapper,
		sqsh_index_t offset, size_t size);

/***************************************
 * mapper/map_manager.c
 */

/**
 * @brief The map manager.
 */
struct SqshMapManager {
	/**
	 * @privatesection
	 */
	struct SqshMapper mapper;
	struct CxLru lru;
	struct CxRcMap maps;
	uint64_t archive_offset;
	sqsh__mutex_t lock;
};

/**
 * @internal
 * @memberof SqshMapManager
 * @brief Initializes a new instance of SqshMapManager.
 *
 * @param[out] manager The SqshMapManager instance to initialize.
 * @param[in] input The input to use.
 * @param[in] config The configuration of this sqsh instance.
 *
 * @return Returns 0 on success, a negative value on error.
 */
SQSH_NO_EXPORT SQSH_NO_UNUSED int sqsh__map_manager_init(
		struct SqshMapManager *manager, const void *input,
		const struct SqshConfig *config);

/**
 * @internal
 * @memberof SqshMapManager
 * Gets the size of the backing archive.
 *
 * @param[in] manager The SqshMapManager instance.
 *
 * @return Returns the size of a chunk.
 */
SQSH_NO_EXPORT uint64_t
sqsh__map_manager_size(const struct SqshMapManager *manager);

/**
 * @internal
 * @memberof SqshMapManager
 * @brief Gets the size of a chunk.
 *
 * @param[in] manager The SqshMapManager instance.
 *
 * @return Returns the size of a chunk.
 */
SQSH_NO_EXPORT size_t
sqsh__map_manager_block_size(const struct SqshMapManager *manager);

/**
 * @internal
 * @memberof SqshMapManager
 * @brief Gets the number of chunks in the file.
 *
 * @param[in] manager The SqshMapManager instance.
 *
 * @return Returns the number of chunks in the file.
 */
SQSH_NO_EXPORT size_t
sqsh__map_manager_block_count(const struct SqshMapManager *manager);

/**
 * @internal
 * @memberof SqshMapManager
 * @brief Gets a map for a chunk.
 *
 * @param[in] manager The SqshMapManager instance.
 * @param[in] index The index of the chunk to map.
 * @param[out] target The mapping to store the result in.
 *
 * @return Returns 0 on success, a negative value on error.
 */
SQSH_NO_EXPORT SQSH_NO_UNUSED int sqsh__map_manager_get(
		struct SqshMapManager *manager, sqsh_index_t index,
		const struct SqshMapSlice **target);

/**
 * @internal
 * @memberof SqshMapManager
 * @brief Releases a map for a chunk.
 *
 * @param[in] manager The SqshMapManager instance.
 * @param[in] mapping The mapping to release.
 *
 * @return Returns 0 on success, a negative value on error.
 */
SQSH_NO_EXPORT int sqsh__map_manager_release(
		struct SqshMapManager *manager, const struct SqshMapSlice *mapping);
/**
 * @internal
 * @memberof SqshMapManager
 * @brief Cleans up the resources used by a SqshMapManager instance.
 *
 * @param[in] manager The SqshMapManager instance to cleanup.
 *
 * @return Returns 0 on success, a negative value on error.
 */
SQSH_NO_EXPORT int sqsh__map_manager_cleanup(struct SqshMapManager *manager);

/***************************************
 * mapper/map_iterator.c
 */

/**
 * @internal
 * @brief An iterator over the chunks in a file.
 */
struct SqshMapIterator {
	/**
	 * @privatesection
	 */
	sqsh_index_t next_index;
	sqsh_index_t segment_count;
	struct SqshMapManager *map_manager;
	const struct SqshMapSlice *mapping;
	const uint8_t *data;
	size_t size;
};

/**
 * @internal
 * @memberof SqshMapIterator
 * @brief Initializes a new instance of SqshMapIterator.
 *
 * @param[out] iterator The SqshMapIterator instance to initialize.
 * @param[in] manager The SqshMapManager instance to use.
 * @param[in] address The address to start the iterator at.
 * @return Returns 0 on success, a negative value on error.
 */
SQSH_NO_EXPORT SQSH_NO_UNUSED int sqsh__map_iterator_init(
		struct SqshMapIterator *iterator, struct SqshMapManager *manager,
		uint64_t address);

/**
 * @internal
 * @memberof SqshMapIterator
 * @brief Moves the iterator to the next chunk.
 *
 * @param[in]  iterator  The SqshMapIterator instance.
 * @param[out] err       Pointer to an int where the error code will be stored.

 *
 * @retval true When the iterator was moved to the next chunk.
 * @retval false When the iterator reached the end of the mapable area or if an
 * error occurred.
 */
SQSH_NO_EXPORT SQSH_NO_UNUSED bool
sqsh__map_iterator_next(struct SqshMapIterator *iterator, int *err);

/**
 * @memberof SqshMapIterator
 * @brief Skips blocks until the block containing the offset is reached.
 * Note that calling this function will invalidate the data pointer returned by
 * sqsh_map_iterator_data().
 *
 * @param[in,out] iterator      The map iterator to skip data in.
 * @param[in]     offset        The offset that is contained in the block to
 * skip to.
 *
 * @return 0 on success, less than 0 on error.
 */
SQSH_NO_EXPORT SQSH_NO_UNUSED int
sqsh__map_iterator_skip(struct SqshMapIterator *iterator, sqsh_index_t *offset);

/**
 * @internal
 * @memberof SqshMapIterator
 * @brief returns the data for the current chunk.
 *
 * @param[in] iterator The SqshMapIterator instance.
 * @return the data for the current chunk.
 */
SQSH_NO_EXPORT const uint8_t *
sqsh__map_iterator_data(const struct SqshMapIterator *iterator);

/**
 * @internal
 * @memberof SqshMapIterator
 * @brief returns the block size of the iterator.
 *
 * @param[in] iterator The SqshMapIterator instance.
 * @return the size of the current chunk.
 */
SQSH_NO_EXPORT size_t
sqsh__map_iterator_block_size(const struct SqshMapIterator *iterator);

/**
 * @internal
 * @memberof SqshMapIterator
 * @brief returns the address of the current chunk.
 *
 * @param[in] iterator The SqshMapIterator instance.
 * @return the address of the current chunk.
 */
SQSH_NO_EXPORT sqsh_index_t
sqsh__map_iterator_address(const struct SqshMapIterator *iterator);

/**
 * @internal
 * @memberof SqshMapIterator
 * @brief returns the size of the current chunk.
 *
 * @param[in] iterator The SqshMapIterator instance.
 * @return the size of the current chunk.
 */
SQSH_NO_EXPORT size_t
sqsh__map_iterator_size(const struct SqshMapIterator *iterator);

/**
 * @internal
 * @memberof SqshMapIterator
 * @brief returns the index of the current chunk.
 *
 * @param[in] iterator The SqshMapIterator instance.
 * @return the index of the current chunk.
 */
SQSH_NO_EXPORT int sqsh__map_iterator_cleanup(struct SqshMapIterator *iterator);

/***************************************
 * mapper/map_reader.c
 */

/**
 * @brief A reader over a mapping of data.
 */
struct SqshMapReader {
	/**
	 * @privatesection
	 */
	uint64_t address;
	uint64_t upper_limit;
	struct SqshMapIterator iterator;
	struct SqshReader reader;
};

/**
 * @internal
 * @memberof SqshMapReader
 * @brief Initializes a mapping reader
 *
 * @param reader The reader to initialize
 * @param mapper The mapper to use
 * @param start_address The start address of the reader
 * @param upper_limit The upper limit of the reader
 * @return 0 on success, negative on error
 */
SQSH_NO_EXPORT SQSH_NO_UNUSED int sqsh__map_reader_init(
		struct SqshMapReader *reader, struct SqshMapManager *mapper,
		const uint64_t start_address, uint64_t upper_limit);

/**
 * @internal
 * @memberof SqshMapReader
 * @brief returns the remaining bytes that can be mapped directly.
 *
 * @param reader The reader to query
 * @return size of the remaining bytes
 */
SQSH_NO_EXPORT size_t
sqsh__map_reader_remaining_direct(const struct SqshMapReader *reader);

/**
 * @internal
 * @memberof SqshMapReader
 * @brief returns the current address of the reader
 *
 * @param reader The reader to query
 * @return the current address of the reader
 *
 */
SQSH_NO_EXPORT SQSH_NO_UNUSED uint64_t
sqsh__map_reader_address(const struct SqshMapReader *reader);

/**
 * @internal
 * @memberof SqshMapReader
 * @brief Advances the reader
 *
 * @param reader The reader to advance
 * @param offset The offset to advance to
 * @param size The size to advance
 * @return 0 on success, negative on error
 */
SQSH_NO_EXPORT SQSH_NO_UNUSED int sqsh__map_reader_advance(
		struct SqshMapReader *reader, sqsh_index_t offset, size_t size);

/**
 * @internal
 * @memberof SqshMapReader
 * @brief Loads all data into the reader. This is useful for
 * tables that must reside completely in memory.
 *
 * @param reader The reader to advance
 * @return 0 on success, negative on error
 */
SQSH_NO_EXPORT SQSH_NO_UNUSED int
sqsh__map_reader_all(struct SqshMapReader *reader);

/**
 * @internal
 * @memberof SqshMapReader
 * @brief Returns the current data of the reader
 *
 * @param reader The reader to get the data from
 * @return The current data of the reader
 */
SQSH_NO_EXPORT const uint8_t *
sqsh__map_reader_data(const struct SqshMapReader *reader);

/**
 * @internal
 * @memberof SqshMapReader
 * @brief Returns the current size of the reader
 *
 * @param reader The reader to get the size from
 * @return The current size of the reader
 */
SQSH_NO_EXPORT size_t sqsh__map_reader_size(const struct SqshMapReader *reader);

/**
 * @internal
 * @memberof SqshMapReader
 * @brief Cleans up the reader
 *
 * @param reader The reader to clean up
 * @return 0 on success, negative on error
 */
SQSH_NO_EXPORT int sqsh__map_reader_cleanup(struct SqshMapReader *reader);

#ifdef __cplusplus
}
#endif
#endif /* SQSH_PRIVATE_MAPPER_H */
