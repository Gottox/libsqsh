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
 * @file         inode_device.c
 */

#include <sqsh_file_private.h>

#include <cextras/memory.h>
#include <sqsh_archive.h>
#include <sqsh_common_private.h>
#include <sqsh_error.h>
#include <sqsh_table.h>

#include <sqsh_data_private.h>
#include <sqsh_tree_private.h>
#include <sys/types.h>

static size_t
inode_device_payload_size(
		const struct SqshDataInode *inode, const struct SqshArchive *archive) {
	(void)inode;
	(void)archive;
	return 0;
}

uint16_t
inode_device_type(const struct SqshDataInode *inode) {
	switch (sqsh__data_inode_type(inode)) {
	case SQSH_INODE_TYPE_EXTENDED_BLOCK:
	case SQSH_INODE_TYPE_BASIC_BLOCK:
		return SQSH_FILE_TYPE_BLOCK;
	default:
		return SQSH_FILE_TYPE_CHAR;
	}
}

static uint32_t
inode_device_hard_link_count(const struct SqshDataInode *inode) {
	return sqsh__data_inode_device_hard_link_count(
			sqsh__data_inode_device(inode));
}

static uint32_t
inode_device_ext_hard_link_count(const struct SqshDataInode *inode) {
	return sqsh__data_inode_device_ext_hard_link_count(
			sqsh__data_inode_device_ext(inode));
}

static uint64_t
inode_device_file_size(const struct SqshDataInode *inode) {
	(void)inode;
	return 0;
}

static uint32_t
inode_device_id(const struct SqshDataInode *inode) {
	return sqsh__data_inode_device_device(sqsh__data_inode_device(inode));
}

static uint32_t
inode_device_ext_id(const struct SqshDataInode *inode) {
	return sqsh__data_inode_device_ext_device(
			sqsh__data_inode_device_ext(inode));
}

static uint32_t
inode_device_ext_xattr_index(const struct SqshDataInode *inode) {
	return sqsh__data_inode_device_ext_xattr_idx(
			sqsh__data_inode_device_ext(inode));
}

const struct SqshInodeImpl sqsh__inode_device_impl = {
		.header_size = sizeof(struct SqshDataInodeDevice),
		.payload_size = inode_device_payload_size,

		.type = inode_device_type,
		.hard_link_count = inode_device_hard_link_count,
		.size = inode_device_file_size,

		.blocks_start = sqsh__file_inode_null_blocks_start,
		.block_size_info = sqsh__file_inode_null_block_size_info,
		.fragment_block_index = sqsh__file_inode_null_fragment_block_index,
		.fragment_block_offset = sqsh__file_inode_null_fragment_block_offset,

		.directory_block_start = sqsh__file_inode_null_directory_block_start,
		.directory_block_offset = sqsh__file_inode_null_directory_block_offset,
		.directory_parent_inode = sqsh__file_inode_null_directory_parent_inode,

		.symlink_target_path = sqsh__file_inode_null_symlink_target_path,
		.symlink_target_size = sqsh__file_inode_null_symlink_target_size,

		.device_id = inode_device_id,

		.xattr_index = sqsh__file_inode_null_xattr_index,
};

const struct SqshInodeImpl sqsh__inode_device_ext_impl = {
		.header_size = sizeof(struct SqshDataInodeDeviceExt),
		.payload_size = inode_device_payload_size,

		.type = inode_device_type,
		.hard_link_count = inode_device_ext_hard_link_count,
		.size = inode_device_file_size,

		.blocks_start = sqsh__file_inode_null_blocks_start,
		.block_size_info = sqsh__file_inode_null_block_size_info,
		.fragment_block_index = sqsh__file_inode_null_fragment_block_index,
		.fragment_block_offset = sqsh__file_inode_null_fragment_block_offset,

		.directory_block_start = sqsh__file_inode_null_directory_block_start,
		.directory_block_offset = sqsh__file_inode_null_directory_block_offset,
		.directory_parent_inode = sqsh__file_inode_null_directory_parent_inode,

		.symlink_target_path = sqsh__file_inode_null_symlink_target_path,
		.symlink_target_size = sqsh__file_inode_null_symlink_target_size,

		.device_id = inode_device_ext_id,

		.xattr_index = inode_device_ext_xattr_index,
};
