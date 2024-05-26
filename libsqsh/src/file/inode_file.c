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
 * @file         inode_file.c
 */

#include <sqsh_file_private.h>

#include <cextras/memory.h>
#include <sqsh_archive.h>
#include <sqsh_common_private.h>
#include <sqsh_error.h>
#include <sqsh_table.h>

#include <sqsh_data_private.h>
#include <sqsh_tree_private.h>

static uint32_t
inode_file_fragment_block_index(const struct SqshDataInode *inode);
static uint32_t
inode_file_ext_fragment_block_index(const struct SqshDataInode *inode);
static uint64_t inode_file_size(const struct SqshDataInode *inode);
static uint64_t inode_file_ext_size(const struct SqshDataInode *inode);

static size_t
calc_block_count(
		const struct SqshArchive *archive, uint64_t file_size,
		uint32_t fragment_index) {
	const struct SqshSuperblock *superblock = sqsh_archive_superblock(archive);
	const uint32_t block_size = sqsh_superblock_block_size(superblock);
	const bool has_fragment = fragment_index != SQSH_INODE_NO_FRAGMENT;

	if (has_fragment) {
		return file_size / block_size;
	} else {
		return SQSH_DIVIDE_CEIL(file_size, block_size);
	}
}

static size_t
inode_file_payload_size(
		const struct SqshDataInode *inode, const struct SqshArchive *archive) {
	const size_t block_count = calc_block_count(
			archive, inode_file_size(inode),
			inode_file_fragment_block_index(inode));
	return block_count * sizeof(uint32_t);
}

static size_t
inode_file_ext_payload_size(
		const struct SqshDataInode *inode, const struct SqshArchive *archive) {
	const size_t block_count = calc_block_count(
			archive, inode_file_ext_size(inode),
			inode_file_ext_fragment_block_index(inode));
	return block_count * sizeof(uint32_t);
}

uint16_t
inode_file_type(const struct SqshDataInode *inode) {
	(void)inode;
	return SQSH_FILE_TYPE_FILE;
}

static uint32_t
inode_regular_hard_link_count(const struct SqshDataInode *inode) {
	(void)inode;
	return 1;
}

static uint32_t
inode_regular_ext_hard_link_count(const struct SqshDataInode *inode) {
	return sqsh__data_inode_file_ext_hard_link_count(
			sqsh__data_inode_file_ext(inode));
}

static uint64_t
inode_file_size(const struct SqshDataInode *inode) {
	return sqsh__data_inode_file_size(sqsh__data_inode_file(inode));
}

static uint64_t
inode_file_ext_size(const struct SqshDataInode *inode) {
	return sqsh__data_inode_file_ext_size(sqsh__data_inode_file_ext(inode));
}

static uint64_t
inode_file_blocks_start(const struct SqshDataInode *inode) {
	return sqsh__data_inode_file_blocks_start(sqsh__data_inode_file(inode));
}

static uint64_t
inode_file_ext_blocks_start(const struct SqshDataInode *inode) {
	return sqsh__data_inode_file_ext_blocks_start(
			sqsh__data_inode_file_ext(inode));
}

static uint32_t
inode_file_block_size_info(
		const struct SqshDataInode *inode, sqsh_index_t index) {
	return sqsh__data_inode_file_block_size_info(
			sqsh__data_inode_file(inode), index);
}

static uint32_t
inode_file_ext_block_size_info(
		const struct SqshDataInode *inode, sqsh_index_t index) {
	return sqsh__data_inode_file_ext_block_size_info(
			sqsh__data_inode_file_ext(inode), index);
}

static uint32_t
inode_file_fragment_block_index(const struct SqshDataInode *inode) {
	return sqsh__data_inode_file_fragment_block_index(
			sqsh__data_inode_file(inode));
}

static uint32_t
inode_file_ext_fragment_block_index(const struct SqshDataInode *inode) {
	return sqsh__data_inode_file_ext_fragment_block_index(
			sqsh__data_inode_file_ext(inode));
}

static uint32_t
inode_file_fragment_block_offset(const struct SqshDataInode *inode) {
	return sqsh__data_inode_file_block_offset(sqsh__data_inode_file(inode));
}

static uint32_t
inode_file_ext_fragment_block_offset(const struct SqshDataInode *inode) {
	return sqsh__data_inode_file_ext_block_offset(
			sqsh__data_inode_file_ext(inode));
}

static uint32_t
inode_file_ext_xattr_index(const struct SqshDataInode *inode) {
	return sqsh__data_inode_file_ext_xattr_idx(
			sqsh__data_inode_file_ext(inode));
}

const struct SqshInodeImpl sqsh__inode_file_impl = {
		.header_size = sizeof(struct SqshDataInodeFile),
		.payload_size = inode_file_payload_size,

		.type = inode_file_type,
		.hard_link_count = inode_regular_hard_link_count,
		.size = inode_file_size,

		.blocks_start = inode_file_blocks_start,
		.block_size_info = inode_file_block_size_info,
		.fragment_block_index = inode_file_fragment_block_index,
		.fragment_block_offset = inode_file_fragment_block_offset,

		.directory_block_start = sqsh__file_inode_null_directory_block_start,
		.directory_block_offset = sqsh__file_inode_null_directory_block_offset,
		.directory_parent_inode = sqsh__file_inode_null_directory_parent_inode,

		.symlink_target_path = sqsh__file_inode_null_symlink_target_path,
		.symlink_target_size = sqsh__file_inode_null_symlink_target_size,

		.device_id = sqsh__file_inode_null_device_id,

		.xattr_index = sqsh__file_inode_null_xattr_index,
};

const struct SqshInodeImpl sqsh__inode_file_ext_impl = {
		.header_size = sizeof(struct SqshDataInodeFileExt),
		.payload_size = inode_file_ext_payload_size,

		.type = inode_file_type,
		.hard_link_count = inode_regular_ext_hard_link_count,
		.size = inode_file_ext_size,

		.blocks_start = inode_file_ext_blocks_start,
		.block_size_info = inode_file_ext_block_size_info,
		.fragment_block_index = inode_file_ext_fragment_block_index,
		.fragment_block_offset = inode_file_ext_fragment_block_offset,

		.directory_block_start = sqsh__file_inode_null_directory_block_start,
		.directory_block_offset = sqsh__file_inode_null_directory_block_offset,
		.directory_parent_inode = sqsh__file_inode_null_directory_parent_inode,

		.symlink_target_path = sqsh__file_inode_null_symlink_target_path,
		.symlink_target_size = sqsh__file_inode_null_symlink_target_size,

		.device_id = sqsh__file_inode_null_device_id,

		.xattr_index = inode_file_ext_xattr_index,
};
