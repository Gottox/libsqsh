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
 * @file         walker.c
 */

#include "../common.h"
#include <utest.h>

#include <sqsh_archive_private.h>
#include <sqsh_data_private.h>
#include <sqsh_tree_private.h>

UTEST(tree_walker, walker_symlink_recursion) {
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
			's', 'r', 'c',
			[DIRECTORY_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			DIRECTORY_HEADER(2, 0, 0),
			DIRECTORY_ENTRY(128, 2, 3, 3),
			's', 'r', 'c',
			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	struct SqshTreeWalker *walker = sqsh_tree_walker_new(&archive, &rv);
	ASSERT_EQ(0, rv);

	rv = sqsh_tree_walker_resolve(walker, "src", true);
	ASSERT_EQ(-SQSH_ERROR_TOO_MANY_SYMLINKS_FOLLOWED, rv);

	sqsh_tree_walker_free(walker);
	sqsh__archive_cleanup(&archive);
}

UTEST(tree_walker, walker_symlink_alternating_recursion) {
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
			INODE_BASIC_SYMLINK(4),
			's', 'r', 'c', '2',
			INODE_HEADER(3, 0, 0, 0, 0, 2),
			INODE_BASIC_SYMLINK(4),
			's', 'r', 'c', '1',
			[DIRECTORY_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			DIRECTORY_HEADER(2, 0, 0),
			DIRECTORY_ENTRY(128, 2, 3, 4),
			's', 'r', 'c', '1',
			DIRECTORY_ENTRY(128, 2, 3, 4),
			's', 'r', 'c', '2',
			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	struct SqshTreeWalker *walker = sqsh_tree_walker_new(&archive, &rv);
	ASSERT_EQ(0, rv);

	rv = sqsh_tree_walker_resolve(walker, "src1", true);
	ASSERT_EQ(-SQSH_ERROR_TOO_MANY_SYMLINKS_FOLLOWED, rv);

	sqsh_tree_walker_free(walker);
	sqsh__archive_cleanup(&archive);
}

UTEST(tree_walker, walker_symlink_open) {
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
			[INODE_TABLE_OFFSET+2+256] =
			INODE_HEADER(2, 0, 0, 0, 0, 3),
			INODE_BASIC_FILE(0, 0xFFFFFFFF, 0, 0),
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

	struct SqshTreeWalker *walker = sqsh_tree_walker_new(&archive, &rv);
	ASSERT_EQ(0, rv);

	rv = sqsh_tree_walker_resolve(walker, "src", true);
	ASSERT_EQ(0, rv);

	const char *name = sqsh_tree_walker_name(walker);
	size_t name_size = sqsh_tree_walker_name_size(walker);
	ASSERT_EQ((size_t)3, name_size);
	ASSERT_EQ(0, memcmp(name, "tgt", name_size));

	sqsh_tree_walker_free(walker);
	sqsh__archive_cleanup(&archive);
}

static void
expect_inode(struct SqshTreeWalker *walker, uint32_t inode_number) {
	int rv;
	struct SqshFile *file = NULL;
	file = sqsh_tree_walker_open_file(walker, &rv);
	assert(rv == 0);
	assert(file != NULL);
	assert(sqsh_file_inode(file) == inode_number);
	sqsh_close(file);
}

UTEST(tree_walker, walker_directory_enter) {
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
			INODE_HEADER(1, 0, 0, 0, 0, 2),
			INODE_BASIC_DIR(256, 1024, 0, 1),

			[DIRECTORY_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			DIRECTORY_HEADER(1, 0, 0),
			DIRECTORY_ENTRY(128, 2, 1, 3),
			'd', 'i', 'r',

			[DIRECTORY_TABLE_OFFSET+128] = METABLOCK_HEADER(0, 128),
			DIRECTORY_HEADER(0, 0, 1),

			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	struct SqshTreeWalker *walker = sqsh_tree_walker_new(&archive, &rv);
	ASSERT_EQ(0, rv);

	rv = sqsh_tree_walker_resolve(walker, "dir", true);
	ASSERT_EQ(0, rv);
	expect_inode(walker, 2);

	rv = sqsh_tree_walker_up(walker);
	ASSERT_EQ(0, rv);
	expect_inode(walker, 1);

	sqsh_tree_walker_free(walker);
	sqsh__archive_cleanup(&archive);
}

UTEST(tree_walker, walker_uninitialized_up) {
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
			DIRECTORY_HEADER(0, 0, 0),

			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	struct SqshTreeWalker walker = {0};
	rv = sqsh__path_resolver_init(&walker.inner, &archive);
	ASSERT_EQ(0, rv);

	rv = sqsh_tree_walker_up(&walker);
	ASSERT_EQ(-SQSH_ERROR_WALKER_CANNOT_GO_UP, rv);

	sqsh__path_resolver_cleanup(&walker.inner);
	sqsh__archive_cleanup(&archive);
}

UTEST(tree_walker, walker_uninitialized_down) {
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
			DIRECTORY_HEADER(0, 0, 0),

			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	struct SqshTreeWalker walker = {0};
	rv = sqsh__path_resolver_init(&walker.inner, &archive);
	ASSERT_EQ(0, rv);

	rv = sqsh_tree_walker_down(&walker);
	ASSERT_EQ(-SQSH_ERROR_WALKER_CANNOT_GO_DOWN, rv);

	sqsh__path_resolver_cleanup(&walker.inner);
	sqsh__archive_cleanup(&archive);
}

UTEST(tree_walker, walker_next) {
	int rv;
	struct SqshArchive archive = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 1024),
			INODE_HEADER(1, 0, 0, 0, 0, 1),
			INODE_BASIC_DIR(0, 45, 0, 0),

			[DIRECTORY_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			DIRECTORY_HEADER(3, 0, 0),
			DIRECTORY_ENTRY(128, 2, 1, 2),
			'e', '1',
			DIRECTORY_ENTRY(128, 2, 1, 2),
			'e', '2',
			DIRECTORY_ENTRY(128, 2, 1, 2),
			'e', '3',
			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	struct SqshTreeWalker walker = {0};
	rv = sqsh__path_resolver_init(&walker.inner, &archive);
	ASSERT_EQ(0, rv);

	rv = sqsh_tree_walker_to_root(&walker);
	ASSERT_EQ(0, rv);

	rv = sqsh_tree_walker_next(&walker);
	assert(rv > 0);

	rv = sqsh_tree_walker_next(&walker);
	assert(rv > 0);

	rv = sqsh_tree_walker_next(&walker);
	assert(rv > 0);

	rv = sqsh_tree_walker_next(&walker);
	ASSERT_EQ(0, rv);

	sqsh__path_resolver_cleanup(&walker.inner);
	sqsh__archive_cleanup(&archive);
}

UTEST_MAIN()
