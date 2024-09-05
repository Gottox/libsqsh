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

#include <sqsh_file_private.h>

#include <cextras/memory.h>
#include <sqsh_archive.h>
#include <sqsh_common_private.h>
#include <sqsh_error.h>
#include <sqsh_table.h>

#include <sqsh_data_private.h>
#include <sqsh_tree_private.h>
#include <stdint.h>

#define SQSH_DEFAULT_MAX_SYMLINKS_FOLLOWED 100

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
		context->type = SQSH_FILE_TYPE_DIRECTORY;
		break;
	case SQSH_INODE_TYPE_BASIC_FILE:
		context->impl = &sqsh__inode_file_impl;
		context->type = SQSH_FILE_TYPE_FILE;
		break;
	case SQSH_INODE_TYPE_BASIC_SYMLINK:
		context->impl = &sqsh__inode_symlink_impl;
		context->type = SQSH_FILE_TYPE_SYMLINK;
		break;
	case SQSH_INODE_TYPE_BASIC_BLOCK:
		context->impl = &sqsh__inode_device_impl;
		context->type = SQSH_FILE_TYPE_BLOCK;
		break;
	case SQSH_INODE_TYPE_BASIC_CHAR:
		context->impl = &sqsh__inode_device_impl;
		context->type = SQSH_FILE_TYPE_CHAR;
		break;
	case SQSH_INODE_TYPE_BASIC_FIFO:
		context->impl = &sqsh__inode_ipc_impl;
		context->type = SQSH_FILE_TYPE_FIFO;
		break;
	case SQSH_INODE_TYPE_BASIC_SOCKET:
		context->impl = &sqsh__inode_ipc_impl;
		context->type = SQSH_FILE_TYPE_SOCKET;
		break;
	case SQSH_INODE_TYPE_EXTENDED_DIRECTORY:
		context->impl = &sqsh__inode_directory_ext_impl;
		context->type = SQSH_FILE_TYPE_DIRECTORY;
		break;
	case SQSH_INODE_TYPE_EXTENDED_FILE:
		context->impl = &sqsh__inode_file_ext_impl;
		context->type = SQSH_FILE_TYPE_FILE;
		break;
	case SQSH_INODE_TYPE_EXTENDED_SYMLINK:
		context->impl = &sqsh__inode_symlink_ext_impl;
		context->type = SQSH_FILE_TYPE_SYMLINK;
		break;
	case SQSH_INODE_TYPE_EXTENDED_BLOCK:
		context->impl = &sqsh__inode_device_ext_impl;
		context->type = SQSH_FILE_TYPE_BLOCK;
		break;
	case SQSH_INODE_TYPE_EXTENDED_CHAR:
		context->impl = &sqsh__inode_device_ext_impl;
		context->type = SQSH_FILE_TYPE_CHAR;
		break;
	case SQSH_INODE_TYPE_EXTENDED_FIFO:
		context->impl = &sqsh__inode_ipc_ext_impl;
		context->type = SQSH_FILE_TYPE_FIFO;
		break;
	case SQSH_INODE_TYPE_EXTENDED_SOCKET:
		context->impl = &sqsh__inode_ipc_ext_impl;
		context->type = SQSH_FILE_TYPE_SOCKET;
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
	if (rv < 0) {
		goto out;
	}

	inode->parent_inode_ref = SQSH_INODE_REF_NULL;

out:
	if (rv < 0) {
		sqsh__file_cleanup(inode);
	}
	return rv;
}

struct SqshFile *
sqsh_open_by_ref(struct SqshArchive *archive, uint64_t inode_ref, int *err) {
	SQSH_NEW_IMPL(sqsh__file_init, struct SqshFile, archive, inode_ref);
}

void
sqsh__file_set_parent_inode_ref(
		struct SqshFile *file, uint64_t parent_inode_ref) {
	file->parent_inode_ref = parent_inode_ref;
}

uint64_t
sqsh__file_parent_inode_ref(struct SqshFile *file, int *err) {
	uint64_t parent_inode_ref = SQSH_INODE_REF_NULL;
	int rv = 0;
	if (file->parent_inode_ref != SQSH_INODE_REF_NULL) {
		parent_inode_ref = file->parent_inode_ref;
		goto out;
	}

	if (sqsh_file_type(file) != SQSH_FILE_TYPE_DIRECTORY) {
		rv = -SQSH_ERROR_INODE_PARENT_UNSET;
		goto out;
	}

	struct SqshInodeMap *inode_map;
	rv = sqsh_archive_inode_map(file->archive, &inode_map);
	if (rv < 0) {
		goto out;
	}

	const uint32_t dir_inode = sqsh_file_directory_parent_inode(file);
	parent_inode_ref = sqsh_inode_map_get2(inode_map, dir_inode, &rv);
	if (rv == -SQSH_ERROR_NO_SUCH_ELEMENT) {
		rv = -SQSH_ERROR_INODE_PARENT_UNSET;
		goto out;
	} else if (rv < 0) {
		goto out;
	}

	file->parent_inode_ref = parent_inode_ref;
out:
	if (err != NULL) {
		*err = rv;
	}

	return parent_inode_ref;
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

uint64_t
sqsh_file_block_count2(const struct SqshFile *context) {
	const struct SqshSuperblock *superblock =
			sqsh_archive_superblock(context->archive);
	uint64_t file_size = sqsh_file_size(context);
	uint32_t block_size = sqsh_superblock_block_size(superblock);

	if (file_size == UINT64_MAX) {
		return UINT64_MAX;
	} else if (file_size == 0) {
		return 0;
	} else if (sqsh_file_has_fragment(context)) {
		return (uint32_t)file_size / block_size;
	} else {
		return (uint32_t)SQSH_DIVIDE_CEIL(file_size, block_size);
	}
}

uint32_t
sqsh_file_block_count(const struct SqshFile *context) {
	uint64_t block_count = sqsh_file_block_count2(context);
	if (block_count > UINT32_MAX) {
		return UINT32_MAX;
	}
	return (uint32_t)block_count;
}

uint32_t
sqsh_file_block_size2(const struct SqshFile *context, uint64_t index) {
	const uint32_t size_info =
			context->impl->block_size_info(get_inode(context), index);

	return sqsh_datablock_size(size_info);
}

uint32_t
sqsh_file_block_size(const struct SqshFile *context, uint32_t index) {
	const uint32_t size_info =
			context->impl->block_size_info(get_inode(context), index);

	return sqsh_datablock_size(size_info);
}

bool
sqsh_file_block_is_compressed2(const struct SqshFile *context, uint64_t index) {
	const uint32_t size_info =
			context->impl->block_size_info(get_inode(context), index);

	return sqsh_datablock_is_compressed(size_info);
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
	return context->type;
}

static int
apply_parent(struct SqshFile *file, struct SqshPathResolver *resolver) {
	int rv = sqsh_path_resolver_up(resolver);
	if (rv == -SQSH_ERROR_WALKER_CANNOT_GO_UP) {
		sqsh__file_set_parent_inode_ref(file, SQSH_INODE_REF_NULL);
	} else if (rv < 0) {
		return rv;
	} else {
		uint64_t new_parent_ref = sqsh_path_resolver_inode_ref(resolver);
		sqsh__file_set_parent_inode_ref(file, new_parent_ref);
	}
	return 0;
}

static int
symlink_resolve(struct SqshFile *context, bool follow_symlinks) {
	int rv = 0;
	struct SqshPathResolver resolver = {0};

	if (sqsh_file_type(context) != SQSH_FILE_TYPE_SYMLINK) {
		return -SQSH_ERROR_NOT_A_SYMLINK;
	}

	const uint64_t old_parent_ref = sqsh__file_parent_inode_ref(context, &rv);
	if (rv < 0) {
		goto out;
	}

	rv = sqsh__path_resolver_init(&resolver, context->archive);
	if (rv < 0) {
		goto out;
	}

	rv = sqsh__path_resolver_to_ref(&resolver, old_parent_ref);
	if (rv < 0) {
		goto out;
	}

	const char *target = sqsh_file_symlink(context);
	const size_t target_size = sqsh_file_symlink_size(context);
	rv = sqsh__path_resolver_resolve_nt(
			&resolver, target, target_size, follow_symlinks);
	if (rv < 0) {
		goto out;
	}

	const uint64_t inode_ref = sqsh_path_resolver_inode_ref(&resolver);
	sqsh__file_cleanup(context);
	rv = sqsh__file_init(context, context->archive, inode_ref);
	if (rv < 0) {
		goto out;
	}

	rv = apply_parent(context, &resolver);
	if (rv < 0) {
		goto out;
	}

out:
	sqsh__path_resolver_cleanup(&resolver);
	return rv;
}

int
sqsh_file_symlink_resolve(struct SqshFile *context) {
	return symlink_resolve(context, false);
}

int
sqsh_file_symlink_resolve_all(struct SqshFile *context) {
	return symlink_resolve(context, true);
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

static struct SqshFile *
open_file(
		struct SqshArchive *archive, const char *path, int *err,
		bool follow_symlink) {
	int rv;
	struct SqshPathResolver resolver = {0};
	struct SqshFile *inode = NULL;
	rv = sqsh__path_resolver_init(&resolver, archive);
	if (rv < 0) {
		goto out;
	}

	rv = sqsh_path_resolver_to_root(&resolver);
	if (rv < 0) {
		goto out;
	}

	rv = sqsh_path_resolver_resolve(&resolver, path, follow_symlink);
	if (rv < 0) {
		goto out;
	}

	inode = sqsh_path_resolver_open_file(&resolver, &rv);
	if (rv < 0) {
		goto out;
	}

	rv = apply_parent(inode, &resolver);
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

struct SqshFile *
sqsh_open(struct SqshArchive *archive, const char *path, int *err) {
	return open_file(archive, path, err, true);
}

struct SqshFile *
sqsh_lopen(struct SqshArchive *archive, const char *path, int *err) {
	return open_file(archive, path, err, false);
}

int
sqsh_close(struct SqshFile *file) {
	SQSH_FREE_IMPL(sqsh__file_cleanup, file);
}
