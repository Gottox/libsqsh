/**
 * @author      : Enno Boland (mail@eboland.de)
 * @file        : sqsh_iterator_private.h
 */

#ifndef SQSH_ITERATOR_PRIVATE_H
#define SQSH_ITERATOR_PRIVATE_H

#include "sqsh_context_private.h"
#include "sqsh_iterator.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////
// iterator/directory_index_iterator.c

struct SqshDirectoryIndexIterator {
	struct SqshInodeContext *inode;
	size_t remaining_entries;
	sqsh_index_t current_offset;
	sqsh_index_t next_offset;
};

/**
 * @internal
 * @brief Initializes an iterator for a directory index
 * @param[in] iterator The iterator to initialize
 * @param[in] inode The inode for the directory to iterate over
 * @return 0 on success, negative value on error
 */
SQSH_NO_UNUSED int sqsh__directory_index_iterator_init(
		struct SqshDirectoryIndexIterator *iterator,
		struct SqshInodeContext *inode);

/**
 * @internal
 * @brief Creates a new iterator for a directory index
 * @param[in] inode The inode for the directory to iterate over
 * @param[out] err Pointer to an int where the error code will be stored
 * @return The newly created iterator on success, NULL on error
 */
SQSH_NO_UNUSED
struct SqshDirectoryIndexIterator *
sqsh__directory_index_iterator_new(struct SqshInodeContext *inode, int *err);

/**
 * @internal
 * @brief Advances the iterator to the next entry in the directory index
 * @param[in] iterator The iterator to advance
 * @return 0 on success, negative value on error
 */
SQSH_NO_UNUSED int sqsh__directory_index_iterator_next(
		struct SqshDirectoryIndexIterator *iterator);

/**
 * @internal
 * @memberof SqshDirectoryIndexIterator
 * @brief Gets the index of the current entry in the directory index
 * @param[in] iterator The iterator to get the index from
 * @return The index of the current entry
 */
uint32_t sqsh__directory_index_iterator_index(
		const struct SqshDirectoryIndexIterator *iterator);

/**
 * @internal
 * @memberof SqshDirectoryIndexIterator
 * @brief Gets the start offset of the current entry in the directory index
 *
 * @param[in] iterator The iterator to get the start offset from
 *
 * @return The start offset of the current entry
 */
uint32_t sqsh__directory_index_iterator_start(
		const struct SqshDirectoryIndexIterator *iterator);

/**
 * @internal
 * @memberof SqshDirectoryIndexIterator
 * @brief Gets the name size of the current entry in the directory index
 *
 * @param[in] iterator The iterator to get the name size from
 *
 * @return The name size of the current entry
 */
uint32_t sqsh__directory_index_iterator_name_size(
		const struct SqshDirectoryIndexIterator *iterator);

/**
 * @internal
 * @memberof SqshDirectoryIndexIterator
 * @brief Gets the name of the current entry in the directory index
 *
 * @param[in] iterator The iterator to get the name from
 *
 * @return The name of the current entry
 */
const char *sqsh__directory_index_iterator_name(
		const struct SqshDirectoryIndexIterator *iterator);

/**
 * @internal
 * @brief Cleans up an iterator for a directory index
 *
 * @param[in] iterator The iterator to clean up
 */
SQSH_NO_UNUSED int sqsh__directory_index_iterator_cleanup(
		struct SqshDirectoryIndexIterator *iterator);

/**
 * @memberof SqshDirectoryIndexIterator
 * @internal
 *
 * Frees the resources used by the given directory index iterator.
 *
 * @param iterator The iterator to free.
 *
 * @return 0 on success, a negative value on error.
 */
int sqsh__directory_index_iterator_free(
		struct SqshDirectoryIndexIterator *iterator);

////////////////////////////////////////
// iterator/directory_iterator.c

struct SqshDirectoryIterator {
	struct SqshInodeContext *inode;
	uint32_t block_start;
	uint32_t block_offset;
	uint32_t size;

	const struct SqshDirectoryFragment *fragments;
	struct SqshDirectoryContext *directory;
	struct SqshMetablockStreamContext metablock;
	size_t remaining_entries;
	sqsh_index_t current_fragment_offset;
	sqsh_index_t next_offset;
	sqsh_index_t current_offset;
};

/**
 * @brief Initializes a directory iterator.
 *
 * @param[out] iterator The iterator to initialize.
 * @param[in]  inode    The inode to iterate through.
 *
 * @return 0 on success, a negative value on error.
 */
SQSH_NO_UNUSED int sqsh__directory_iterator_init(
		struct SqshDirectoryIterator *iterator, struct SqshInodeContext *inode);

/**
 * @brief Cleans up resources used by a directory iterator.
 *
 * @param[in] iterator The iterator to cleanup.
 *
 * @return 0 on success, a negative value on error.
 */
int sqsh__directory_iterator_cleanup(struct SqshDirectoryIterator *iterator);

////////////////////////////////////////
// iterator/xattr_iterator.c

struct SqshXattrIterator {
	struct SqshMetablockStreamContext metablock;
	struct SqshMetablockStreamContext out_of_line_value;
	struct SqshXattrTable *context;
	int remaining_entries;
	sqsh_index_t next_offset;
	sqsh_index_t key_offset;
	sqsh_index_t value_offset;
};

/**
 * @brief Initializes a new xattr iterator.
 *
 * @param[out] iterator The iterator to initialize.
 * @param[in]  inode    The inode to iterate through xattrs.
 *
 * @return 0 on success, a negative value on error.
 */
SQSH_NO_UNUSED int sqsh__xattr_iterator_init(
		struct SqshXattrIterator *iterator,
		const struct SqshInodeContext *inode);

/**
 * @brief Cleans up resources used by an xattr iterator.
 *
 * @param[in] iterator The iterator to cleanup.
 *
 * @return 0 on success, a negative value on error.
 */
int sqsh__xattr_iterator_cleanup(struct SqshXattrIterator *iterator);

#ifdef __cplusplus
}
#endif
#endif /* end of include guard SQSH_ITERATOR_PRIVATE_H */
