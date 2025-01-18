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
#include <utest.h>

#include <sqsh_archive_private.h>
#include <sqsh_common_private.h>
#include <sqsh_easy.h>
#include <sqsh_tree_private.h>

UTEST(ease_file, test_file_get_content_through_symlink) {
	struct SqshArchive archive = {0};
	uint8_t payload[8192] = {
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

	uint8_t *content = sqsh_easy_file_content(&archive, "/src", NULL);
	ASSERT_NE(NULL, content);
	ASSERT_EQ(0, strcmp((char *)content, "12345678"));
	free(content);

	sqsh__archive_cleanup(&archive);
}

UTEST(ease_file, test_file_exists_through_dead_symlink) {
	int rv = 0;
	struct SqshArchive archive = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			[1024] = '1', '2', '3', '4', '5', '6', '7', '8',
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 1024),
			INODE_HEADER(1, 0, 0, 0, 0, 1),
			INODE_BASIC_DIR(0, 26, 0, 0),
			[INODE_TABLE_OFFSET+2+128] =
			INODE_HEADER(3, 0, 0, 0, 0, 2),
			INODE_BASIC_SYMLINK(3),
			't', 'g', 't',
			[DIRECTORY_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			DIRECTORY_HEADER(1, 0, 0),
			DIRECTORY_ENTRY(128, 2, 3, 3),
			's', 'r', 'c',
			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	bool exists = sqsh_easy_file_exists(&archive, "/src", &rv);
	ASSERT_EQ(false, exists);
	ASSERT_EQ(0, rv);

	sqsh__archive_cleanup(&archive);
}

UTEST(ease_file, test_file_exists_through_symlink) {
	struct SqshArchive archive = {0};
	int rv = 0;
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			[1024] = '1', '2', '3', '4', '5', '6', '7', '8',
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 1024),
			INODE_HEADER(1, 0, 0, 0, 0, 1),
			INODE_BASIC_DIR(0, 37, 0, 0),
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

	bool exists = sqsh_easy_file_exists(&archive, "/src", &rv);
	assert(exists);
	ASSERT_EQ(0, rv);

	exists = sqsh_easy_file_exists(&archive, "/file_not_found", &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(false, exists);

	sqsh__archive_cleanup(&archive);
}

UTEST(ease_file, test_file_size) {
	struct SqshArchive archive = {0};
	int rv = 0;
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 1024),
			INODE_HEADER(1, 0, 0, 0, 0, 1),
			INODE_BASIC_DIR(0, 27, 0, 0),
			[INODE_TABLE_OFFSET+2+128] =
			INODE_HEADER(2, 0, 0, 0, 0, 3),
			INODE_BASIC_FILE(1024, 0xFFFFFFFF, 0, 424242),
			DATA_BLOCK_REF(8, 0),
			[DIRECTORY_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			DIRECTORY_HEADER(1, 0, 0),
			DIRECTORY_ENTRY(128, 2, 2, 4),
			'f', 'i', 'l', 'e',
			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	size_t file_size = sqsh_easy_file_size(&archive, "/file", &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ((size_t)424242, file_size);

	file_size = sqsh_easy_file_size(&archive, "/file_not_found", &rv);
	ASSERT_EQ(-SQSH_ERROR_NO_SUCH_FILE, rv);
	ASSERT_EQ((size_t)0, file_size);

	sqsh__archive_cleanup(&archive);
}

UTEST(ease_file, test_file_permission) {
	struct SqshArchive archive = {0};
	int rv = 0;
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 1024),
			INODE_HEADER(1, 0, 0, 0, 0, 1),
			INODE_BASIC_DIR(0, 27, 0, 0),
			[INODE_TABLE_OFFSET+2+128] =
			INODE_HEADER(2, 0666, 0, 0, 0, 3),
			INODE_BASIC_FILE(1024, 0xFFFFFFFF, 0, 424242),
			DATA_BLOCK_REF(8, 0),
			[DIRECTORY_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			DIRECTORY_HEADER(1, 0, 0),
			DIRECTORY_ENTRY(128, 2, 2, 4),
			'f', 'i', 'l', 'e',
			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	mode_t permission = sqsh_easy_file_permission(&archive, "/file", &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ((size_t)0666, permission);

	permission = sqsh_easy_file_permission(&archive, "/file_not_found", &rv);
	ASSERT_EQ(-SQSH_ERROR_NO_SUCH_FILE, rv);
	ASSERT_EQ((size_t)0, permission);

	sqsh__archive_cleanup(&archive);
}

UTEST(ease_file, test_file_mtime) {
	struct SqshArchive archive = {0};
	int rv = 0;
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 1024),
			INODE_HEADER(1, 0, 0, 0, 0, 1),
			INODE_BASIC_DIR(0, 27, 0, 0),
			[INODE_TABLE_OFFSET+2+128] =
			INODE_HEADER(2, 0666, 0, 0, 42424242, 3),
			INODE_BASIC_FILE(1024, 0xFFFFFFFF, 0, 424242),
			DATA_BLOCK_REF(8, 0),
			[DIRECTORY_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			DIRECTORY_HEADER(1, 0, 0),
			DIRECTORY_ENTRY(128, 2, 2, 4),
			'f', 'i', 'l', 'e',
			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	time_t mtime = sqsh_easy_file_mtime(&archive, "/file", &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(42424242, mtime);

	mtime = sqsh_easy_file_mtime(&archive, "/file_not_found", &rv);
	ASSERT_EQ(-SQSH_ERROR_NO_SUCH_FILE, rv);
	ASSERT_EQ(0, mtime);

	sqsh__archive_cleanup(&archive);
}

UTEST_MAIN()
