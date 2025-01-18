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
 * @file         traversal.c
 */

#include "../common.h"
#include <stdint.h>
#include <utest.h>

#include <sqsh_archive_private.h>
#include <sqsh_data_private.h>
#include <sqsh_tree.h>
#include <sqsh_tree_private.h>

UTEST(traversal, test_recursive_directory) {
	int rv;
	struct SqshArchive archive = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			INODE_HEADER(1, 0, 0, 0, 0, 1),
			INODE_BASIC_DIR(0, 1024, 0, 2),

			[INODE_TABLE_OFFSET + 128] = METABLOCK_HEADER(0, 128),
			INODE_HEADER(1, 0, 0, 0, 0, 2),
			INODE_BASIC_DIR(128, 1024, 0, 1),

			[DIRECTORY_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			DIRECTORY_HEADER(1, 128, 2),
			DIRECTORY_ENTRY(0, 0, 1, 1),
			'r',

			[DIRECTORY_TABLE_OFFSET + 128] = METABLOCK_HEADER(0, 128),
			DIRECTORY_HEADER(1, 0, 1),
			DIRECTORY_ENTRY(0, 0, 1, 1),
			'r',

			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	struct SqshFile file = {0};
	rv = sqsh__file_init(&file, &archive, 0);
	ASSERT_EQ(0, rv);

	struct SqshTreeTraversal traversal = {0};
	rv = sqsh__tree_traversal_init(&traversal, &file);
	ASSERT_EQ(0, rv);

	for (int i = 0; i < 128; i++) {
		bool has_next = sqsh_tree_traversal_next(&traversal, &rv);
		ASSERT_EQ(0, rv);
		ASSERT_TRUE(has_next);
	}
	bool has_next = sqsh_tree_traversal_next(&traversal, &rv);
	ASSERT_EQ(-SQSH_ERROR_DIRECTORY_RECURSION, rv);
	assert(!has_next);

	sqsh__tree_traversal_cleanup(&traversal);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST(traversal, test_empty_dir) {
	int rv;
	struct SqshArchive archive = {0};
	char *path;
	uint8_t payload[] = {
			/* clang-format off */
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 1024),
			INODE_HEADER(1, 0, 0, 0, 0, 1),
			INODE_BASIC_DIR(0, 3, 0, 0),
			[DIRECTORY_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			DIRECTORY_HEADER(1, 0, 0),
			[FRAGMENT_TABLE_OFFSET] = 0,
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	struct SqshFile file = {0};
	rv = sqsh__file_init(&file, &archive, 0);
	ASSERT_EQ(0, rv);

	struct SqshTreeTraversal traversal = {0};
	rv = sqsh__tree_traversal_init(&traversal, &file);
	ASSERT_EQ(0, rv);

	bool has_next = sqsh_tree_traversal_next(&traversal, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_TRUE(has_next);
	ASSERT_EQ(
			(enum SqshTreeTraversalState)
					SQSH_TREE_TRAVERSAL_STATE_DIRECTORY_BEGIN,
			sqsh_tree_traversal_state(&traversal));
	path = sqsh_tree_traversal_path_dup(&traversal);
	ASSERT_STREQ("", path);
	free(path);

	has_next = sqsh_tree_traversal_next(&traversal, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_TRUE(has_next);
	ASSERT_EQ(
			(enum SqshTreeTraversalState)
					SQSH_TREE_TRAVERSAL_STATE_DIRECTORY_END,
			sqsh_tree_traversal_state(&traversal));
	path = sqsh_tree_traversal_path_dup(&traversal);
	ASSERT_STREQ("", path);
	free(path);

	has_next = sqsh_tree_traversal_next(&traversal, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_FALSE(has_next);

	sqsh__tree_traversal_cleanup(&traversal);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST_MAIN()
