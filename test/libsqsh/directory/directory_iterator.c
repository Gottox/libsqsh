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
 * @file         directory_iterator.c
 */

#include "../common.h"
#include <stdint.h>
#include <utest.h>

#include <sqsh_archive_private.h>
#include <sqsh_data_private.h>
#include <sqsh_directory.h>
#include <sqsh_directory_private.h>

UTEST(directory_iterator, iter_invalid_file_name_with_0) {
	int rv;
	struct SqshArchive archive = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 1024),
			INODE_HEADER(1, 0, 0, 0, 0, 1),
			INODE_BASIC_DIR(0, 1024, 0, 0),
			[DIRECTORY_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			DIRECTORY_HEADER(1, 0, 0),
			DIRECTORY_ENTRY(128, 2, 3, 6),
			'\0', 'H', 'e', 'l', 'l', 'o',
			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	struct SqshFile file = {0};
	rv = sqsh__file_init(&file, &archive, 0);
	ASSERT_EQ(0, rv);

	struct SqshDirectoryIterator iter = {0};
	rv = sqsh__directory_iterator_init(&iter, &file);
	ASSERT_EQ(0, rv);

	bool has_next = sqsh_directory_iterator_next(&iter, &rv);
	ASSERT_EQ(-SQSH_ERROR_CORRUPTED_DIRECTORY_ENTRY, rv);
	ASSERT_EQ(false, has_next);

	sqsh__directory_iterator_cleanup(&iter);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST(directory_iterator, iter_invalid_file_name_with_slash) {
	int rv;
	struct SqshArchive archive = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 1024),
			INODE_HEADER(1, 0, 0, 0, 0, 1),
			INODE_BASIC_DIR(0, 1024, 0, 0),
			[DIRECTORY_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			DIRECTORY_HEADER(1, 0, 0),
			DIRECTORY_ENTRY(128, 2, 3, 11),
			'/', 'e', 't', 'c', '/', 'p', 'a', 's', 's', 'w', 'd',
			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	struct SqshFile file = {0};
	rv = sqsh__file_init(&file, &archive, 0);
	ASSERT_EQ(0, rv);

	struct SqshDirectoryIterator iter = {0};
	rv = sqsh__directory_iterator_init(&iter, &file);
	ASSERT_EQ(0, rv);

	bool has_next = sqsh_directory_iterator_next(&iter, &rv);
	ASSERT_EQ(-SQSH_ERROR_CORRUPTED_DIRECTORY_ENTRY, rv);
	ASSERT_EQ(false, has_next);

	sqsh__directory_iterator_cleanup(&iter);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST(directory_iterator, iter_two_files) {
	int rv;
	struct SqshArchive archive = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 1024),
			INODE_HEADER(1, 0, 0, 0, 0, 1),
			INODE_BASIC_DIR(0, 33, 0, 0),
			[INODE_TABLE_OFFSET+2+128] =
			INODE_HEADER(3, 0, 0, 0, 0, 2),
			INODE_BASIC_SYMLINK(3),
			't', 'g', 't',
			[INODE_TABLE_OFFSET+2+256] =
			INODE_HEADER(2, 0, 0, 0, 0, 3),
			INODE_BASIC_FILE(0, 0xFFFFFFFF, 0, 0),
			[DIRECTORY_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			DIRECTORY_HEADER(2, 0, 0),
			DIRECTORY_ENTRY(128, 2, 3, 1),
			'1',
			DIRECTORY_ENTRY(256, 3, 2, 1),
			'2',
			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	struct SqshFile file = {0};
	rv = sqsh__file_init(&file, &archive, 0);
	ASSERT_EQ(0, rv);

	struct SqshDirectoryIterator iter = {0};
	rv = sqsh__directory_iterator_init(&iter, &file);
	ASSERT_EQ(0, rv);

	bool has_next = sqsh_directory_iterator_next(&iter, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(true, has_next);

	const char *name = sqsh_directory_iterator_name(&iter);
	size_t size = sqsh_directory_iterator_name_size(&iter);
	ASSERT_EQ((size_t)1, size);
	ASSERT_EQ('1', name[0]);

	has_next = sqsh_directory_iterator_next(&iter, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(true, has_next);

	name = sqsh_directory_iterator_name(&iter);
	size = sqsh_directory_iterator_name_size(&iter);
	ASSERT_EQ((size_t)1, size);
	ASSERT_EQ('2', name[0]);

	has_next = sqsh_directory_iterator_next(&iter, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(false, has_next);

	sqsh__directory_iterator_cleanup(&iter);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST(directory_iterator, iter_invalid_file_type) {
	int rv;
	struct SqshArchive archive = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 1024),
			INODE_HEADER(3, 0, 0, 0, 0, 1),
			INODE_BASIC_SYMLINK(3),
			't', 'g', 't',

			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	struct SqshFile file = {0};
	rv = sqsh__file_init(&file, &archive, 0);
	ASSERT_EQ(0, rv);

	struct SqshDirectoryIterator *iter =
			sqsh_directory_iterator_new(&file, &rv);
	ASSERT_EQ(-SQSH_ERROR_NOT_A_DIRECTORY, rv);
	ASSERT_EQ(NULL, iter);

	sqsh_directory_iterator_free(iter);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST(directory_iterator, iter_inconsistent_file_type) {
	int rv;
	struct SqshArchive archive = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 1024),
			INODE_HEADER(1, 0, 0, 0, 0, 1),
			INODE_BASIC_DIR(0, 1024, 0, 0),
			[INODE_TABLE_OFFSET+2+128] =
			INODE_HEADER(3, 0, 0, 0, 0, 2),
			INODE_BASIC_SYMLINK(3),
			't', 'g', 't',
			[DIRECTORY_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			DIRECTORY_HEADER(2, 0, 0),
			DIRECTORY_ENTRY(128, 2, 1, 1),
			'1',
			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	struct SqshFile file = {0};
	rv = sqsh__file_init(&file, &archive, 0);
	ASSERT_EQ(0, rv);

	struct SqshDirectoryIterator *iter =
			sqsh_directory_iterator_new(&file, &rv);
	ASSERT_EQ(0, rv);

	bool has_next = sqsh_directory_iterator_next(iter, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(true, has_next);

	struct SqshFile *entry_file = sqsh_directory_iterator_open_file(iter, &rv);
	ASSERT_EQ(-SQSH_ERROR_CORRUPTED_DIRECTORY_ENTRY, rv);
	ASSERT_EQ(NULL, entry_file);

	sqsh_directory_iterator_free(iter);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST(directory_iterator, iter_over_corrupt_header_too_small) {
	int rv;
	struct SqshArchive archive = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 1024),
			INODE_HEADER(1, 0, 0, 0, 0, 1),
			INODE_BASIC_DIR(0, 10, 0, 0),
			[DIRECTORY_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			DIRECTORY_HEADER(2, 0, 0),
			DIRECTORY_ENTRY(128, 2, 3, 1),
			'1',
			DIRECTORY_ENTRY(256, 3, 2, 1),
			'2',
			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	struct SqshFile file = {0};
	rv = sqsh__file_init(&file, &archive, 0);
	ASSERT_EQ(0, rv);

	struct SqshDirectoryIterator iter = {0};
	rv = sqsh__directory_iterator_init(&iter, &file);
	ASSERT_EQ(0, rv);

	bool has_next = sqsh_directory_iterator_next(&iter, &rv);
	ASSERT_EQ(-SQSH_ERROR_CORRUPTED_DIRECTORY_HEADER, rv);
	ASSERT_EQ(false, has_next);

	sqsh__directory_iterator_cleanup(&iter);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST(directory_iterator, iter_inode_overflow) {
	int rv;
	struct SqshArchive archive = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 1024),
			INODE_HEADER(1, 0, 0, 0, 0, 1),
			INODE_BASIC_DIR(0, 1024, 0, 0),
			[DIRECTORY_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			DIRECTORY_HEADER(1, 0, UINT32_MAX),
			DIRECTORY_ENTRY(128, 3, 3, 1),
			'd',
			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	struct SqshFile file = {0};
	rv = sqsh__file_init(&file, &archive, 0);
	ASSERT_EQ(0, rv);

	struct SqshDirectoryIterator iter = {0};
	rv = sqsh__directory_iterator_init(&iter, &file);
	ASSERT_EQ(0, rv);

	bool has_next = sqsh_directory_iterator_next(&iter, &rv);
	ASSERT_EQ(-SQSH_ERROR_CORRUPTED_DIRECTORY_ENTRY, rv);
	ASSERT_EQ(false, has_next);

	sqsh__directory_iterator_cleanup(&iter);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST(directory_iterator, iter_inode_underflow) {
	int rv;
	struct SqshArchive archive = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 1024),
			INODE_HEADER(1, 0, 0, 0, 0, 1),
			INODE_BASIC_DIR(0, 1024, 0, 0),
			[DIRECTORY_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			DIRECTORY_HEADER(1, 0, 12),
			DIRECTORY_ENTRY(128, -13, 3, 1),
			'd',
			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	struct SqshFile file = {0};
	rv = sqsh__file_init(&file, &archive, 0);
	ASSERT_EQ(0, rv);

	struct SqshDirectoryIterator iter = {0};
	rv = sqsh__directory_iterator_init(&iter, &file);
	ASSERT_EQ(0, rv);

	bool has_next = sqsh_directory_iterator_next(&iter, &rv);
	ASSERT_EQ(-SQSH_ERROR_CORRUPTED_DIRECTORY_ENTRY, rv);
	ASSERT_EQ(false, has_next);

	sqsh__directory_iterator_cleanup(&iter);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST(directory_iterator, iter_inode_to_zero) {
	int rv;
	struct SqshArchive archive = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 1024),
			INODE_HEADER(1, 0, 0, 0, 0, 1),
			INODE_BASIC_DIR(0, 1024, 0, 0),
			[DIRECTORY_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			DIRECTORY_HEADER(1, 0, 12),
			DIRECTORY_ENTRY(128, -12, 3, 1),
			'd',
			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	struct SqshFile file = {0};
	rv = sqsh__file_init(&file, &archive, 0);
	ASSERT_EQ(0, rv);

	struct SqshDirectoryIterator iter = {0};
	rv = sqsh__directory_iterator_init(&iter, &file);
	ASSERT_EQ(0, rv);

	bool has_next = sqsh_directory_iterator_next(&iter, &rv);
	ASSERT_EQ(-SQSH_ERROR_CORRUPTED_DIRECTORY_ENTRY, rv);
	ASSERT_EQ(false, has_next);

	sqsh__directory_iterator_cleanup(&iter);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST_MAIN()
