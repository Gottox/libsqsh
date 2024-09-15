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

#ifndef SQSH_FILE_H
#define SQSH_FILE_H

#include "sqsh_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct SqshFile;
struct SqshArchive;

/***************************************
 * file/file.c
 */

/**
 * @brief Value that indicates the absence of a fragment.
 */
#define SQSH_INODE_NO_FRAGMENT 0xFFFFFFFF
/**
 * @brief Value that indicates the absence of an xattr table.
 */
#define SQSH_INODE_NO_XATTR 0xFFFFFFFF

__attribute__((deprecated(
		"Since 1.5.0. libsqsh should never return this."))) static const int
		SQSH_FILE_TYPE_UNKNOWN = -1;
/**
 * @brief enum that represents the file type.
 */
enum SqshFileType {
	/* avoid overlapping with the types in inode_data.h */
	SQSH_FILE_TYPE_DIRECTORY = 1 + (1 << 8),
	SQSH_FILE_TYPE_FILE,
	SQSH_FILE_TYPE_SYMLINK,
	SQSH_FILE_TYPE_BLOCK,
	SQSH_FILE_TYPE_CHAR,
	SQSH_FILE_TYPE_FIFO,
	SQSH_FILE_TYPE_SOCKET
};

/**
 * @memberof SqshFile
 * @brief Initialize the file context from a path.
 *
 * @param[in] archive The sqsh archive context.
 * @param[in] path The path the file or directory.
 * @param[out] err Pointer to an int where the error code will be stored.
 *
 * @return 0 on success, less than 0 on error.
 */
SQSH_NO_UNUSED struct SqshFile *
sqsh_open(struct SqshArchive *archive, const char *path, int *err);

/**
 * @memberof SqshFile
 * @brief Initialize the file context from a path. This function is identical to
 * `sqsh_open()` but if the path is a symlink, the symlink target not resolved.
 *
 * @param[in] archive The sqsh archive context.
 * @param[in] path The path the file or directory.
 * @param[out] err Pointer to an int where the error code will be stored.
 *
 * @return 0 on success, less than 0 on error.
 */
SQSH_NO_UNUSED struct SqshFile *
sqsh_lopen(struct SqshArchive *archive, const char *path, int *err);

/**
 * @memberof SqshFile
 * @brief Initializes a file context in heap
 *
 * @param archive The sqsh context to use.
 * @param inode_ref The inode reference to initialize the context with.
 * @param[out] err Pointer to an int where the error code will be stored.
 *
 * @return a pointer to the sqsh context or NULL if an error occurred.
 */
SQSH_NO_UNUSED struct SqshFile *
sqsh_open_by_ref(struct SqshArchive *archive, uint64_t inode_ref, int *err);

/**
 * @memberof SqshFile
 * @brief returns whether the file is an extended structure.
 *
 * @param[in] context The file context.
 *
 * @return true if the file is an extended structure.
 */
bool sqsh_file_is_extended(const struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief Getter for the inode hard link count.
 *
 * @param[in] context The file context.
 *
 * @return the amount of hard links to the inode.
 */
uint32_t sqsh_file_hard_link_count(const struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief Getter for the file size. 0 if the file has no size.
 *
 * @param[in] context The file context.
 *
 * @return the file type.
 */
uint64_t sqsh_file_size(const struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief Getter for the permissions of the file.
 *
 * @param[in] context The file context.
 *
 * @return the permissions of the file.
 */
uint16_t sqsh_file_permission(const struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief Getter for the inode number.
 *
 * @param[in] context The file context.
 *
 * @return the inode number.
 */
uint32_t sqsh_file_inode(const struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief Getter for the file modification time in seconds since epoch.
 *
 * @param[in] context The file context.
 *
 * @return the file modification time.
 */
uint32_t sqsh_file_modified_time(const struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief Getter for the start block of the file content. This is only
 * internally used and will be used while retrieving the file content.
 *
 * @param[in] context The file context.
 *
 * @return the start block of the file content or UINT64_MAX if the type
 * is not SQSH_FILE_TYPE_FILE.
 */
uint64_t sqsh_file_blocks_start(const struct SqshFile *context);

/**
 * @deprecated Since 1.5.0. Use sqsh_file_blockcount2() instead.
 * @memberof SqshFile
 * @brief Getter for the amount of blocks of the file content. This is only
 * internally used and will be used while retrieving the file content.
 *
 * @param[in] context The file context.
 *
 * @return the amount of blocks of the file content. If the file is not of
 * of type SQSH_FILE_TYPE_FILE, UINT32_MAX will be returned.
 */
__attribute__((deprecated("Since 1.5.0. Use sqsh_file_blockcount2() instead.")))
uint32_t
sqsh_file_block_count(const struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief Getter for the amount of blocks of the file content. This is only
 * internally used and will be used while retrieving the file content.
 *
 * @param[in] context The file context.
 *
 * @return the amount of blocks of the file content. If the file is not of
 * of type SQSH_FILE_TYPE_FILE, UINT32_MAX will be returned.
 */
uint64_t sqsh_file_block_count2(const struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief Getter the size of a block of the file content. This is only
 * internally used and will be used while retrieving the file content.
 *
 * @param[in] context The file context.
 * @param index The index of the block.
 *
 * @return the size of the block with the index.
 */
uint32_t sqsh_file_block_size2(const struct SqshFile *context, uint64_t index);

/**
 * @deprecated Since 1.6.0. Use sqsh_file_block_size2() instead.
 * @memberof SqshFile
 * @brief Getter the size of a block of the file content. This is only
 * internally used and will be used while retrieving the file content.
 *
 * @param[in] context The file context.
 * @param index The index of the block.
 *
 * @return the size of the block with the index.
 */
__attribute__((deprecated("Since 1.6.0. Use sqsh_file_block_size2() instead.")))
uint32_t
sqsh_file_block_size(const struct SqshFile *context, uint32_t index);

/**
 * @deprecated Since 1.6.0. Use sqsh_file_block_is_compressed2() instead.
 * @memberof SqshFile
 * @brief Checks whether a certain block is compressed.
 *
 * @param[in] context The file context.
 * @param index The index of the block.
 *
 * @return true if the block is compressed, false otherwise.
 */
__attribute__((deprecated(
		"Since 1.6.0. Use sqsh_file_block_is_compressed2() instead."))) bool
sqsh_file_block_is_compressed(const struct SqshFile *context, uint32_t index);

/**
 * @memberof SqshFile
 * @brief Checks whether a certain block is compressed.
 *
 * @param[in] context The file context.
 * @param index The index of the block.
 *
 * @return true if the block is compressed, false otherwise.
 */
bool
sqsh_file_block_is_compressed2(const struct SqshFile *context, uint64_t index);

/**
 * @memberof SqshFile
 * @brief retrieve the fragment block index. This is only internally used
 *
 * and will be used while retrieving the file content.
 * @param[in] context The file context.
 *
 * @return the fragment block index.
 */
uint32_t sqsh_file_fragment_block_index(const struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief retrieve the fragment block offset. This is only internally used
 * and will be used while retrieving the file content.
 *
 * @param[in] context The file context.
 *
 * @return the offset inside of the fragment block.
 */
uint32_t sqsh_file_fragment_block_offset(const struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief retrieve the directory block start. This is only internally used
 * and will be used while iterating over the directory entries.
 *
 * @param[in] context The file context.
 *
 * @return the directory block start.
 */
uint32_t sqsh_file_directory_block_start(const struct SqshFile *context);

/**
 * @deprecated Since 1.3.0. Use sqsh_file_directory_block_offset2() instead.
 * @memberof SqshFile
 * @brief retrieve the directory block offset. This is only internally used
 * and will be used while iterating over the directory entries.
 *
 * @param[in] context The file context.
 *
 * @return the directory block offset.
 */
__attribute__((
		deprecated("Since 1.3.0. Use sqsh_file_directory_block_offset2() "
				   "instead."))) uint32_t
sqsh_file_directory_block_offset(const struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief retrieve the directory block offset. This is only internally used
 * and will be used while iterating over the directory entries.
 *
 * @param[in] context The file context.
 *
 * @return the directory block offset.
 */
uint16_t sqsh_file_directory_block_offset2(const struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief retrieve the parent inode of the directory.
 *
 * @param[in] context The file context.
 *
 * @return the directory block offset.
 */
uint32_t sqsh_file_directory_parent_inode(const struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief returns true if the file has a fragment block.
 *
 * @param[in] context The file context.
 *
 * @return true if the file has a fragment block, false otherwise.
 */
bool sqsh_file_has_fragment(const struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief returns the type of the file.
 *
 * @param[in] context The file context.
 *
 * @return the type of the file.
 */
enum SqshFileType sqsh_file_type(const struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief resolves the symlink target. After calling this function the file is
 * in place changed to the target of the symlink.
 *
 * @param[in] context The file context.
 *
 * @return int 0 on success, less than 0 on error.
 */
int sqsh_file_symlink_resolve(struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief resolves all symlink target targets until a file is hit. This function
 * is similar to sqsh_file_symlink_resolve() but resolves symlinks recursively
 * until a file is hit.
 *
 * @param[in] context The file context.
 *
 * @return int 0 on success, less than 0 on error.
 */
int sqsh_file_symlink_resolve_all(struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief returns the target of a symbolic link. Be aware that the returned
 * value is not zero terminated.
 *
 * To get the length of the target use sqsh_file_symlink_size().
 *
 * If you need a zero terminated string use sqsh_file_symlink_dup().
 *
 * @param[in] context The file context.
 *
 * @return the target of a symbolic link, NULL if the file is not a symbolic
 * link.
 */
const char *sqsh_file_symlink(const struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief creates a heap allocated copy of the target of a symbolic link.
 *
 * The caller is responsible for calling free() on the returned pointer.
 *
 * The returned string is 0 terminated.
 *
 * @param[in] context The file context.
 *
 * @return the target of a symbolic link, NULL if the file is not a symbolic
 * link.
 */
SQSH_NO_UNUSED char *sqsh_file_symlink_dup(const struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief returns the length of the target of a symbolic link in bytes.
 *
 * @param[in] context The file context.
 *
 * @return the length of the target of a symbolic link in bytes or 0 if the
 * file is not a symbolic link.
 */
uint32_t sqsh_file_symlink_size(const struct SqshFile *context);

/**
 * @memberof SqshFile
 *
 * @brief returns the device id of the device inode.
 *
 * @param[in] context The file context.
 *
 * @return the device id of the device inode or 0 if the file is not a device.
 */
uint32_t sqsh_file_device_id(const struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief returns the owner user id of the file.
 *
 * @param[in] context The file context.
 *
 * @return the owner uid of the file.
 */
uint32_t sqsh_file_uid(const struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief returns the owner group id of the file.
 *
 * @param[in] context The file context.
 *
 * @return the owner gid of the file.
 */
uint32_t sqsh_file_gid(const struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief returns the inode reference to this file.
 *
 * The owner reference is an encoded physical location of the inode inside of
 * the archive.
 *
 * To decode this value use the following code:
 *
 * ```
 * uint64_t ref = sqsh_file_ref(context);
 * uint64_t outer_address = ref >> 16;
 * uint64_t inner_address = ref & 0xffff;
 * ```
 *
 * The outer address is the physical location of the metablock within the
 * archive.
 *
 * The inner address is the physical location of the inode inside of the
 * decompressed metablock.
 *
 * @param[in] context The file context.
 *
 * @return the reference to this file.
 */
uint64_t sqsh_file_inode_ref(const struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief returns index of the extended attribute inside of the xattr table.
 *
 * @param[in] context The file context.
 *
 * @return the index of the extended attribute inside of the xattr table.
 */
uint32_t sqsh_file_xattr_index(const struct SqshFile *context);

/**
 * @memberof SqshFile
 * @brief cleans up an file context and frees the memory.
 *
 * @param[in] file The file context.
 *
 * @return int 0 on success, less than 0 on error.
 */
int sqsh_close(struct SqshFile *file);

#ifdef __cplusplus
}
#endif
#endif /* SQSH_FILE_H */
