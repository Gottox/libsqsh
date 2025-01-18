/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2023, Enno Boland
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @author       Enno Boland (mail@eboland.de)
 * @file         file.c
 */

#include "../common.h"
#include <stdint.h>
#include <utest.h>

#include <sqsh_archive_private.h>
#include <sqsh_common_private.h>
#include <sqsh_data_private.h>
#include <sqsh_file_private.h>

UTEST(file, load_file) {
	int rv;
	struct SqshArchive archive = {0};
	struct SqshFile file = {0};
	uint8_t payload[8192] = {
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET + 15] = METABLOCK_HEADER(0, 128),
			0,
			0,
			0,
			INODE_HEADER(2, 0666, 0, 0, 4242, 1),
			INODE_BASIC_FILE(1024, 0xFFFFFFFF, 0, 1),
			UINT32_BYTES(42),

	};
	mk_stub(&archive, payload, sizeof(payload));

	uint64_t inode_ref = sqsh_address_ref_create(15, 3);
	rv = sqsh__file_init(&file, &archive, inode_ref);
	ASSERT_EQ(0, rv);

	ASSERT_EQ(SQSH_FILE_TYPE_FILE, (int)sqsh_file_type(&file));
	ASSERT_EQ(0666, sqsh_file_permission(&file));
	ASSERT_EQ((uint32_t)4242, sqsh_file_modified_time(&file));
	ASSERT_EQ((uint32_t)1024, sqsh_file_blocks_start(&file));
	ASSERT_EQ(false, sqsh_file_has_fragment(&file));

	ASSERT_EQ((uint32_t)1, sqsh_file_block_count(&file));
	ASSERT_EQ((uint32_t)42, sqsh_file_block_size(&file, 0));

	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST(file, resolve_file) {
	int rv = 0;
	struct SqshArchive archive = {0};
	uint8_t payload[] = {
			/* clang-format off */
			SQSH_HEADER,
			[1024] = '1', '2', '3', '4', '5', '6', '7', '8',
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 1024),
			INODE_HEADER(1, 0, 0, 0, 0, 1),
			INODE_BASIC_DIR(0, 1024, 0, 0),
			[INODE_TABLE_OFFSET+2+128] =
			INODE_HEADER(3, 0, 0, 0, 0, 2),
			INODE_BASIC_SYMLINK(3),
			't', 'g', 't',
			[INODE_TABLE_OFFSET+2+256] =
			INODE_HEADER(2, 0, 0, 0, 0, 3),
			INODE_BASIC_FILE(1024, 0xFFFFFFFF, 0, 8),
			DATA_BLOCK_REF(8, 0),
			[DIRECTORY_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			DIRECTORY_HEADER(2, 0, 0),
			DIRECTORY_ENTRY(128, 2, 3, 3),
			's', 'r', 'c',
			DIRECTORY_ENTRY(256, 3, 2, 3),
			't', 'g', 't',
			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	struct SqshFile *symlink = sqsh_lopen(&archive, "/src", &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(SQSH_FILE_TYPE_SYMLINK, (int)sqsh_file_type(symlink));

	rv = sqsh_file_symlink_resolve(symlink);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(SQSH_FILE_TYPE_FILE, (int)sqsh_file_type(symlink));
	ASSERT_EQ((uint32_t)3, sqsh_file_inode(symlink));

	sqsh_close(symlink);

	sqsh__archive_cleanup(&archive);
}

UTEST(file, resolve_unkown_dir_inode) {
	int rv = 0;
	struct SqshArchive archive = {0};
	uint8_t payload[] = {
			/* clang-format off */
			SQSH_HEADER,
			[1024] = '1', '2', '3', '4', '5', '6', '7', '8',
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 1024),
			INODE_HEADER(1, 0, 0, 0, 0, 1),
			INODE_BASIC_DIR(0, 1024, 0, 0),
			[INODE_TABLE_OFFSET+2+128] =
			INODE_HEADER(3, 0, 0, 0, 0, 2),
			INODE_BASIC_SYMLINK(3),
			't', 'g', 't',
			[INODE_TABLE_OFFSET+2+256] =
			INODE_HEADER(2, 0, 0, 0, 0, 3),
			INODE_BASIC_FILE(1024, 0xFFFFFFFF, 0, 8),
			DATA_BLOCK_REF(8, 0),
			[DIRECTORY_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			DIRECTORY_HEADER(2, 0, 0),
			DIRECTORY_ENTRY(128, 2, 3, 3),
			's', 'r', 'c',
			DIRECTORY_ENTRY(256, 3, 2, 3),
			't', 'g', 't',
			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	struct SqshFile *symlink = sqsh_lopen(&archive, "/src", &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(SQSH_FILE_TYPE_SYMLINK, (int)sqsh_file_type(symlink));
	symlink->parent_inode_ref = UINT64_MAX;

	rv = sqsh_file_symlink_resolve(symlink);
	ASSERT_EQ(-SQSH_ERROR_INODE_PARENT_UNSET, rv);

	sqsh_close(symlink);
	sqsh__archive_cleanup(&archive);
}

UTEST(file, get_ids) {
	int rv;
	struct SqshArchive archive = {0};
	struct SqshFile file = {0};
	uint8_t payload[8192] = {
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET + 15] = METABLOCK_HEADER(0, 128),
			INODE_HEADER(2, 0666, 0, 0, 4242, 1),
			INODE_BASIC_FILE(1024, 0xFFFFFFFF, 0, 1),
			UINT32_BYTES(42),
			[ID_TABLE_OFFSET] = UINT64_BYTES(0),
			METABLOCK_HEADER(0, 128),
			UINT32_BYTES(0),
			UINT32_BYTES(0),
	};
	mk_stub(&archive, payload, sizeof(payload));

	uint64_t inode_ref = sqsh_address_ref_create(15, 0);
	rv = sqsh__file_init(&file, &archive, inode_ref);
	ASSERT_EQ(0, rv);

	ASSERT_EQ((uint32_t)123, sqsh_file_gid(&file));

	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST_MAIN()
