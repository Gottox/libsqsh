/**
 * @author      : Enno Boland (mail@eboland.de)
 * @file        : inode
 * @created     : Thursday May 06, 2021 15:22:00 CEST
 */

#include "../utils.h"
#include "metablock_context.h"

#include <stdint.h>
#include <stdlib.h>

#ifndef SQUASH_INODE_CONTEXT_H

#define SQUASH_INODE_CONTEXT_H

#define SQUASH_INODE_NO_FRAGMENT 0xFFFFFFFF

struct SquashSuperblock;
struct SquashInode;
struct SquashInodeTable;

enum SquashInodeContextType {
	SQUASH_INODE_TYPE_UNKNOWN = -1,
	// avoid overlapping with the types in ../data/inode.h
	SQUASH_INODE_TYPE_DIRECTORY = 1 + (1 << 8),
	SQUASH_INODE_TYPE_FILE,
	SQUASH_INODE_TYPE_SYMLINK,
	SQUASH_INODE_TYPE_BLOCK,
	SQUASH_INODE_TYPE_CHAR,
	SQUASH_INODE_TYPE_FIFO,
	SQUASH_INODE_TYPE_SOCKET,
};

struct SquashInodeContext {
	struct SquashInode *inode;
	struct SquashMetablockContext extract;
	uint32_t datablock_block_size;
};

struct SquashInodeDirectoryIndexIterator {
	struct SquashInodeContext *inode;
	const struct SquashInodeDirectoryIndex *indices;
	size_t remaining_entries;
	off_t offset;
};

SQUASH_NO_UNUSED int squash_inode_load(struct SquashInodeContext *inode,
		const struct SquashSuperblock *superblock, uint64_t inode_ref);

SQUASH_NO_UNUSED uint32_t squash_inode_hard_link_count(
		struct SquashInodeContext *inode);

uint64_t squash_inode_file_size(struct SquashInodeContext *inode);
uint16_t squash_inode_permission(struct SquashInodeContext *inode);
uint32_t squash_inode_modified_time(struct SquashInodeContext *inode);
uint64_t squash_inode_file_blocks_start(struct SquashInodeContext *inode);
// TODO: Find right datatype for index
uint32_t squash_inode_file_block_size(
		struct SquashInodeContext *inode, int index);
uint32_t squash_inode_file_fragment_block_index(
		struct SquashInodeContext *inode);
uint32_t squash_inode_file_fragment_block_offset(
		struct SquashInodeContext *inode);

enum SquashInodeContextType squash_inode_type(struct SquashInodeContext *inode);

uint32_t squash_inode_hard_link_count(struct SquashInodeContext *inode);

const char *squash_inode_symlink(struct SquashInodeContext *inode);
SQUASH_NO_UNUSED int squash_inode_symlink_dup(
		struct SquashInodeContext *inode, char **namebuffer);
uint32_t squash_inode_symlink_size(struct SquashInodeContext *inode);

int squash_inode_cleanup(struct SquashInodeContext *inode);

SQUASH_NO_UNUSED int squash_inode_directory_iterator_init(
		struct SquashInodeDirectoryIndexIterator *iterator,
		struct SquashInodeContext *inode);
SQUASH_NO_UNUSED const struct SquashInodeDirectoryIndex *
squash_inode_directory_index_iterator_next(
		struct SquashInodeDirectoryIndexIterator *iterator);
SQUASH_NO_UNUSED int squash_inode_directory_index_iterator_clean(
		struct SquashInodeDirectoryIndexIterator *iterator);

void squash_inode_ref_to_block(
		uint64_t ref, uint32_t *block_index, uint16_t *offset);
SQUASH_NO_UNUSED uint64_t squash_inode_ref_from_block(
		uint32_t block_index, uint16_t offset);

#endif /* end of include guard SQUASH_INODE_CONTEXT_H */
