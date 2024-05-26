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
 * @file         file.c
 */

#include "sqsh_file.h"
#include <sqsh_file_private.h>

#include <cextras/memory.h>
#include <sqsh_archive.h>
#include <sqsh_common_private.h>
#include <sqsh_error.h>
#include <sqsh_table.h>

#include <sqsh_data_private.h>
#include <sqsh_tree_private.h>
#include <stdint.h>

static const struct SqshDataInode *
get_inode(const struct SqshFile *inode) {
	return (const struct SqshDataInode *)sqsh__metablock_reader_data(
			&inode->metablock);
}

static int
inode_load(struct SqshFile *context) {
	int rv = 0;

	const struct SqshDataInode *inode = get_inode(context);
	const enum SqshDataInodeType type = sqsh__data_inode_type(inode);
	switch (type) {
	case SQSH_INODE_TYPE_BASIC_DIRECTORY:
		context->impl = &sqsh__inode_directory_impl;
		break;
	case SQSH_INODE_TYPE_BASIC_FILE:
		context->impl = &sqsh__inode_file_impl;
		break;
	case SQSH_INODE_TYPE_BASIC_SYMLINK:
		context->impl = &sqsh__inode_symlink_impl;
		break;
	case SQSH_INODE_TYPE_BASIC_BLOCK:
	case SQSH_INODE_TYPE_BASIC_CHAR:
		context->impl = &sqsh__inode_device_impl;
		break;
	case SQSH_INODE_TYPE_BASIC_FIFO:
	case SQSH_INODE_TYPE_BASIC_SOCKET:
		context->impl = &sqsh__inode_ipc_impl;
		break;
	case SQSH_INODE_TYPE_EXTENDED_DIRECTORY:
		context->impl = &sqsh__inode_directory_ext_impl;
		break;
	case SQSH_INODE_TYPE_EXTENDED_FILE:
		context->impl = &sqsh__inode_file_ext_impl;
		break;
	case SQSH_INODE_TYPE_EXTENDED_SYMLINK:
		context->impl = &sqsh__inode_symlink_ext_impl;
		break;
	case SQSH_INODE_TYPE_EXTENDED_BLOCK:
	case SQSH_INODE_TYPE_EXTENDED_CHAR:
		context->impl = &sqsh__inode_device_ext_impl;
		break;
	case SQSH_INODE_TYPE_EXTENDED_SOCKET:
	case SQSH_INODE_TYPE_EXTENDED_FIFO:
		context->impl = &sqsh__inode_ipc_ext_impl;
		break;
	default:
		return -SQSH_ERROR_UNKNOWN_FILE_TYPE;
	}
	size_t size =
			sizeof(struct SqshDataInodeHeader) + context->impl->header_size;
	rv = sqsh__metablock_reader_advance(&context->metablock, 0, size);
	if (rv < 0) {
		return rv;
	}

	/* The pointer may has been invalidated by reader_advance, so retrieve
	 * it again.
	 */
	inode = get_inode(context);
	size += context->impl->payload_size(inode, context->archive);

	rv = sqsh__metablock_reader_advance(&context->metablock, 0, size);

	return rv;
}

int
sqsh__file_init(
		struct SqshFile *inode, struct SqshArchive *archive,
		uint64_t inode_ref) {
	const uint64_t outer_offset = sqsh_address_ref_outer_offset(inode_ref);
	const uint16_t inner_offset = sqsh_address_ref_inner_offset(inode_ref);
	uint64_t address_outer;
	struct SqshInodeMap *inode_map;

	int rv = 0;
	const struct SqshSuperblock *superblock = sqsh_archive_superblock(archive);

	const uint64_t inode_table_start =
			sqsh_superblock_inode_table_start(superblock);

	if (SQSH_ADD_OVERFLOW(inode_table_start, outer_offset, &address_outer)) {
		rv = -SQSH_ERROR_INTEGER_OVERFLOW;
		goto out;
	}
	const uint64_t upper_limit =
			sqsh_superblock_directory_table_start(superblock);
	rv = sqsh__metablock_reader_init(
			&inode->metablock, archive, address_outer, upper_limit);
	if (rv < 0) {
		goto out;
	}
	rv = sqsh__metablock_reader_advance(
			&inode->metablock, inner_offset,
			sizeof(struct SqshDataInodeHeader));
	if (rv < 0) {
		goto out;
	}

	inode->archive = archive;
	inode->inode_ref = inode_ref;

	rv = inode_load(inode);
	if (rv < 0) {
		goto out;
	}
	rv = sqsh_archive_inode_map(archive, &inode_map);
	if (rv < 0) {
		goto out;
	}
	rv = sqsh_inode_map_set2(inode_map, sqsh_file_inode(inode), inode_ref);

out:
	if (rv < 0) {
		sqsh__file_cleanup(inode);
	}
	return rv;
}

struct SqshFile *
sqsh_open_by_ref(struct SqshArchive *sqsh, uint64_t inode_ref, int *err) {
	int rv = 0;
	struct SqshFile *inode = calloc(1, sizeof(struct SqshFile));
	if (inode == NULL) {
		rv = -SQSH_ERROR_MALLOC_FAILED;
		goto out;
	}
	rv = sqsh__file_init(inode, sqsh, inode_ref);
	if (rv < 0) {
		free(inode);
		inode = NULL;
	}
out:
	if (err != NULL) {
		*err = rv;
	}
	return inode;
}

bool
sqsh_file_is_extended(const struct SqshFile *context) {
	// Only extended inodes have pointers to the xattr table. Instead of relying
	// on a dedicated field, we reuse this property to decide if the inode is
	// extended.
	return context->impl->xattr_index != &sqsh__file_inode_null_xattr_index;
}

uint32_t
sqsh_file_hard_link_count(const struct SqshFile *context) {
	return context->impl->hard_link_count(get_inode(context));
}

uint64_t
sqsh_file_size(const struct SqshFile *context) {
	return context->impl->size(get_inode(context));
}

uint16_t
sqsh_file_permission(const struct SqshFile *inode) {
	return sqsh__data_inode_permissions(get_inode(inode));
}

uint32_t
sqsh_file_inode(const struct SqshFile *inode) {
	return sqsh__data_inode_number(get_inode(inode));
}

uint32_t
sqsh_file_modified_time(const struct SqshFile *inode) {
	return sqsh__data_inode_modified_time(get_inode(inode));
}

uint64_t
sqsh_file_blocks_start(const struct SqshFile *context) {
	return context->impl->blocks_start(get_inode(context));
}

// TODO: reconsider the return type of this function.
uint32_t
sqsh_file_block_count(const struct SqshFile *context) {
	const struct SqshSuperblock *superblock =
			sqsh_archive_superblock(context->archive);
	uint64_t file_size = sqsh_file_size(context);
	uint32_t block_size = sqsh_superblock_block_size(superblock);

	if (file_size == UINT64_MAX) {
		return UINT32_MAX;
	} else if (file_size == 0) {
		return 0;
	} else if (sqsh_file_has_fragment(context)) {
		return (uint32_t)file_size / block_size;
	} else {
		return (uint32_t)SQSH_DIVIDE_CEIL(file_size, block_size);
	}
}

uint32_t
sqsh_file_block_size(const struct SqshFile *context, uint32_t index) {
	const uint32_t size_info =
			context->impl->block_size_info(get_inode(context), index);

	return sqsh_datablock_size(size_info);
}

bool
sqsh_file_block_is_compressed(const struct SqshFile *context, uint32_t index) {
	const uint32_t size_info =
			context->impl->block_size_info(get_inode(context), index);

	return sqsh_datablock_is_compressed(size_info);
}

uint32_t
sqsh_file_fragment_block_index(const struct SqshFile *context) {
	return context->impl->fragment_block_index(get_inode(context));
}

uint32_t
sqsh_file_directory_block_start(const struct SqshFile *context) {
	return context->impl->directory_block_start(get_inode(context));
}

uint32_t
sqsh_file_directory_block_offset(const struct SqshFile *context) {
	return context->impl->directory_block_offset(get_inode(context));
}

uint16_t
sqsh_file_directory_block_offset2(const struct SqshFile *context) {
	return context->impl->directory_block_offset(get_inode(context));
}

uint32_t
sqsh_file_directory_parent_inode(const struct SqshFile *context) {
	return context->impl->directory_parent_inode(get_inode(context));
}

uint32_t
sqsh_file_fragment_block_offset(const struct SqshFile *context) {
	return context->impl->fragment_block_offset(get_inode(context));
}

bool
sqsh_file_has_fragment(const struct SqshFile *inode) {
	return sqsh_file_fragment_block_index(inode) != SQSH_INODE_NO_FRAGMENT;
}

enum SqshFileType
sqsh_file_type(const struct SqshFile *context) {
	return context->impl->type(get_inode(context));
}

const char *
sqsh_file_symlink(const struct SqshFile *context) {
	return context->impl->symlink_target_path(get_inode(context));
}

char *
sqsh_file_symlink_dup(const struct SqshFile *inode) {
	const size_t size = sqsh_file_symlink_size(inode);
	const char *link_target = sqsh_file_symlink(inode);

	return cx_memdup(link_target, size);
}

uint32_t
sqsh_file_symlink_size(const struct SqshFile *context) {
	return context->impl->symlink_target_size(get_inode(context));
}

uint32_t
sqsh_file_device_id(const struct SqshFile *context) {
	return context->impl->device_id(get_inode(context));
}

static uint32_t
inode_get_id(const struct SqshFile *context, sqsh_index_t idx) {
	int rv = 0;
	struct SqshIdTable *id_table;
	uint32_t id;

	rv = sqsh_archive_id_table(context->archive, &id_table);
	if (rv < 0) {
		return UINT32_MAX;
	}

	rv = sqsh_id_table_get(id_table, idx, &id);
	if (rv < 0) {
		return UINT32_MAX;
	}
	return id;
}

uint32_t
sqsh_file_uid(const struct SqshFile *context) {
	return inode_get_id(context, sqsh__data_inode_uid_idx(get_inode(context)));
}

uint32_t
sqsh_file_gid(const struct SqshFile *context) {
	return inode_get_id(context, sqsh__data_inode_gid_idx(get_inode(context)));
}

uint64_t
sqsh_file_inode_ref(const struct SqshFile *context) {
	return context->inode_ref;
}

uint32_t
sqsh_file_xattr_index(const struct SqshFile *context) {
	return context->impl->xattr_index(get_inode(context));
}

int
sqsh__file_cleanup(struct SqshFile *inode) {
	return sqsh__metablock_reader_cleanup(&inode->metablock);
}

struct SqshFile *
sqsh_open(struct SqshArchive *archive, const char *path, int *err) {
	int rv;
	struct SqshPathResolver resolver = {0};
	struct SqshFile *inode = NULL;
	rv = sqsh__path_resolver_init(&resolver, archive);
	if (rv < 0) {
		goto out;
	}

	rv = sqsh_path_resolver_resolve(&resolver, path, true);
	if (rv < 0) {
		goto out;
	}

	inode = sqsh_path_resolver_open_file(&resolver, &rv);
	if (rv < 0) {
		goto out;
	}

out:
	if (err != NULL) {
		*err = rv;
	}
	sqsh__path_resolver_cleanup(&resolver);
	return inode;
}

int
sqsh_close(struct SqshFile *file) {
	if (file == NULL) {
		return 0;
	}
	int rv = sqsh__file_cleanup(file);
	free(file);
	return rv;
}
