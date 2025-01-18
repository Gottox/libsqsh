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
 * @file         common.h
 */

#ifndef COMMON_H
#define COMMON_H

#include <assert.h>
#include <sqsh_archive.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 *  _______________    __ byte 0
 * |               |
 * |  Superblock   |
 * |_______________|   __ byte 96
 * |               |
 * |  Compression  |
 * |    options    |
 * |_______________|   __ byte 1024
 * |               |
 * |  Data blocks  |
 * |  & fragments  |
 * |_______________|   __ byte 2048
 * |               |
 * |  Inode table  |
 * |_______________|   __ byte 3072
 * |               |
 * |   Directory   |
 * |     table     |
 * |_______________|   __ byte 4096
 * |               |
 * |   Fragment    |
 * |    table      |
 * |_______________|   __ byte 5120
 * |               |
 * | Export table  |
 * |_______________|   __ byte 6144
 * |               |
 * |    UID/GID    |
 * |  lookup table |
 * |_______________|   __ byte 7168
 * |               |
 * |     Xattr     |
 * |     table     |
 * |_______________|   __ byte 8192
 */
#define COMPRESS_OPTIONS_OFFSET 96
#define INODE_TABLE_OFFSET 2048
#define DIRECTORY_TABLE_OFFSET 3072
#define FRAGMENT_TABLE_OFFSET 4096
#define EXPORT_TABLE_OFFSET 5120
#define ID_TABLE_OFFSET 6144
#define XATTR_TABLE_OFFSET 7168

#if __BYTE_ORDER == __LITTLE_ENDIAN
#	define UINT16_BYTES(x) (uint8_t)(x), (uint8_t)(((uint64_t)(x)) >> 8)
#	define UINT32_BYTES(x) \
		UINT16_BYTES((uint16_t)(x)), \
				UINT16_BYTES((uint16_t)(((uint32_t)(x)) >> 16))
#	define UINT64_BYTES(x) \
		UINT32_BYTES((uint32_t)(x)), \
				UINT32_BYTES((uint32_t)(((uint64_t)(x)) >> 32))
#else
#	define UINT16_BYTES(x) (uint8_t)((x) >> 8), (uint8_t)(x)
#	define UINT32_BYTES(x) \
		UINT16_BYTES((uint16_t)((x) >> 16)), UINT16_BYTES((uint16_t)(x))
#	define UINT64_BYTES(x) \
		UINT32_BYTES((uint16_t)((x) >> 32)), UINT16_BYTES((uint16_t)(x))
#endif

#define SQSH_HEADER \
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
			0, 0, 0, 0, 0
#define METABLOCK_HEADER(c, s) UINT16_BYTES(((c) ? 0 : 0x8000) + (s))
#define INODE_HEADER(t, p, uid, gid, mtime, nbr) \
	UINT16_BYTES(t), UINT16_BYTES(p), UINT16_BYTES(uid), UINT16_BYTES(gid), \
			UINT32_BYTES(mtime), UINT32_BYTES(nbr)
#define INODE_BASIC_FILE(block_start, frag_index, block_offset, file_size) \
	UINT32_BYTES(block_start), UINT32_BYTES(frag_index), \
			UINT32_BYTES(block_offset), UINT32_BYTES(file_size)
#define INODE_BASIC_DIR(block_index, file_size, block_offset, parent_inode) \
	UINT32_BYTES(block_index), UINT32_BYTES(1 /* link count */), \
			UINT16_BYTES(file_size), UINT16_BYTES(block_offset), \
			UINT32_BYTES(parent_inode)
#define INODE_EXT_DIR( \
		block_index, file_size, block_offset, parent_inode, index_count, \
		xattr_index) \
	UINT32_BYTES(1 /* link count */), UINT32_BYTES(file_size), \
			UINT32_BYTES(block_index), UINT32_BYTES(parent_inode), \
			UINT32_BYTES(index_count), UINT32_BYTES(block_offset), \
			UINT32_BYTES(xattr_index)

#define INODE_BASIC_SYMLINK(target_size) \
	UINT32_BYTES(1 /* link count */), UINT32_BYTES(target_size)
#define DATA_BLOCK_REF(size, compressed) \
	UINT32_BYTES(size | (compressed ? 0x0 : (1 << 24)))

#define DIRECTORY_HEADER(count, start, inode_number) \
	UINT32_BYTES(count - 1), UINT32_BYTES(start), UINT32_BYTES(inode_number)
#define DIRECTORY_ENTRY(offset, inode_offset, type, name_size) \
	UINT16_BYTES(offset), UINT16_BYTES(inode_offset), UINT16_BYTES(type), \
			UINT16_BYTES((name_size)-1)

#define XATTR_LOOKUP_HEADER(kv_start, count) \
	UINT64_BYTES(kv_start), UINT32_BYTES(count), UINT32_BYTES(0)
#define XATTR_LOOKUP_ENTRY(xattr_index, xattr_off, count, size) \
	UINT64_BYTES(((xattr_index) << 16) | (xattr_off)), UINT32_BYTES(count), \
			UINT32_BYTES(size)
#define XATTR_NAME_HEADER(type, size) UINT16_BYTES(type), UINT16_BYTES(size)
#define XATTR_VALUE_HEADER(size) UINT32_BYTES(size)

#define ZLIB_ABCD \
	0x78, 0x9c, 0x4b, 0x4c, 0x4a, 0x4e, 0x01, 0x00, 0x03, 0xd8, 0x01, 0x8b
#define ZLIB_EFGH \
	0x78, 0x9c, 0x4b, 0x4d, 0x4b, 0xcf, 0x00, 0x00, 0x04, 0x00, 0x01, 0x9b

#define CHUNK_SIZE(...) sizeof((uint8_t[]){__VA_ARGS__})

#define DEFAULT_MAPPER sqsh_mapper_impl_static
/* We're using a ridiculously small block size to
 * test the mappers ability to handle small blocks.
 */
#define DEFAULT_BLOCK_SIZE ~0

#define DEFAULT_CONFIG(s) \
	(struct SqshConfig) { \
		.source_size = (s), .source_mapper = DEFAULT_MAPPER, \
		.mapper_block_size = DEFAULT_BLOCK_SIZE, \
	}

#define LENGTH(x) (sizeof(x) / sizeof(x[0]))

const uint8_t *
mk_stub(struct SqshArchive *sqsh, uint8_t *payload, size_t payload_size);

FILE *test_sqsh_prepare_archive(uint8_t *payload, size_t payload_size);

void test_sqsh_init_archive(
		struct SqshArchive *sqsh, FILE *farchive, uint8_t *payload,
		size_t payload_size);

#endif /* !COMMON_H */
