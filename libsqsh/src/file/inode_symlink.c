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
 * @file         inode_symlink.c
 */

#include <sqsh_file_private.h>

#include <cextras/memory.h>
#include <sqsh_archive.h>
#include <sqsh_common_private.h>
#include <sqsh_error.h>
#include <sqsh_table.h>

#include <sqsh_data_private.h>
#include <sqsh_tree_private.h>
#include <stdint.h>

static uint32_t inode_symlink_target_size(const struct SqshDataInode *inode);
static uint32_t
inode_symlink_ext_target_size(const struct SqshDataInode *inode);

uint16_t
inode_symlink_type(const struct SqshDataInode *inode) {
	(void)inode;
	return SQSH_FILE_TYPE_SYMLINK;
}

/* payload_size */
static size_t
inode_symlink_payload_size(
		const struct SqshDataInode *inode, const struct SqshArchive *archive) {
	(void)archive;
	return inode_symlink_target_size(inode);
}

static size_t
inode_symlink_ext_payload_size(
		const struct SqshDataInode *inode, const struct SqshArchive *archive) {
	(void)archive;
	return inode_symlink_ext_target_size(inode);
}

/* hard_link_count */
static uint32_t
inode_symlink_hard_link_count(const struct SqshDataInode *inode) {
	return sqsh__data_inode_symlink_hard_link_count(
			sqsh__data_inode_symlink(inode));
}

static uint32_t
inode_symlink_ext_hard_link_count(const struct SqshDataInode *inode) {
	return sqsh__data_inode_symlink_ext_hard_link_count(
			sqsh__data_inode_symlink_ext(inode));
}

/* size */
static uint64_t
inode_symlink_size(const struct SqshDataInode *inode) {
	return inode_symlink_target_size(inode);
}

static uint64_t
inode_symlink_ext_size(const struct SqshDataInode *inode) {
	return inode_symlink_ext_target_size(inode);
}

static const char *
inode_symlink_target_path(const struct SqshDataInode *inode) {
	return (const char *)sqsh__data_inode_symlink_ext_target_path(
			sqsh__data_inode_symlink_ext(inode));
}

static uint32_t
inode_symlink_ext_target_size(const struct SqshDataInode *inode) {
	return sqsh__data_inode_symlink_ext_target_size(
			sqsh__data_inode_symlink_ext(inode));
}

static uint32_t
inode_symlink_target_size(const struct SqshDataInode *inode) {
	return sqsh__data_inode_symlink_target_size(
			sqsh__data_inode_symlink(inode));
}

static const char *
inode_symlink_ext_target_path(const struct SqshDataInode *inode) {
	return (const char *)sqsh__data_inode_symlink_ext_target_path(
			sqsh__data_inode_symlink_ext(inode));
}

uint32_t
inode_symlink_ext_xattr_index(const struct SqshDataInode *inode) {
	return sqsh__data_inode_symlink_ext_xattr_idx(
			sqsh__data_inode_symlink_ext(inode));
}

const struct SqshInodeImpl sqsh__inode_symlink_impl = {
		.header_size = sizeof(struct SqshDataInodeSymlink),
		.payload_size = inode_symlink_payload_size,

		.type = inode_symlink_type,
		.hard_link_count = inode_symlink_hard_link_count,
		.size = inode_symlink_size,

		.blocks_start = sqsh__file_inode_null_blocks_start,
		.block_size_info = sqsh__file_inode_null_block_size_info,
		.fragment_block_index = sqsh__file_inode_null_fragment_block_index,
		.fragment_block_offset = sqsh__file_inode_null_fragment_block_offset,

		.directory_block_start = sqsh__file_inode_null_directory_block_start,
		.directory_block_offset = sqsh__file_inode_null_directory_block_offset,
		.directory_parent_inode = sqsh__file_inode_null_directory_parent_inode,

		.symlink_target_path = inode_symlink_target_path,
		.symlink_target_size = inode_symlink_target_size,

		.device_id = sqsh__file_inode_null_device_id,

		.xattr_index = sqsh__file_inode_null_xattr_index,
};

const struct SqshInodeImpl sqsh__inode_symlink_ext_impl = {
		.header_size = sizeof(struct SqshDataInodeSymlinkExt),
		.payload_size = inode_symlink_ext_payload_size,

		.type = inode_symlink_type,
		.hard_link_count = inode_symlink_ext_hard_link_count,
		.size = inode_symlink_ext_size,

		.blocks_start = sqsh__file_inode_null_blocks_start,
		.block_size_info = sqsh__file_inode_null_block_size_info,
		.fragment_block_index = sqsh__file_inode_null_fragment_block_index,
		.fragment_block_offset = sqsh__file_inode_null_fragment_block_offset,

		.directory_block_start = sqsh__file_inode_null_directory_block_start,
		.directory_block_offset = sqsh__file_inode_null_directory_block_offset,
		.directory_parent_inode = sqsh__file_inode_null_directory_parent_inode,

		.symlink_target_path = inode_symlink_ext_target_path,
		.symlink_target_size = inode_symlink_ext_target_size,

		.device_id = sqsh__file_inode_null_device_id,

		.xattr_index = inode_symlink_ext_xattr_index,
};
