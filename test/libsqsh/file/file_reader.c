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
 * @file         file_reader.c
 */

#include "../common.h"
#include <utest.h>

#include <sqsh_archive_private.h>
#include <sqsh_common_private.h>
#include <sqsh_content_private.h>
#include <sqsh_data_private.h>
#include <sqsh_file.h>

UTEST(file_reader, load_file_from_compressed_data_block) {
	int rv;
	struct SqshArchive archive = {0};
	struct SqshFile file = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* datablock */
			[1024] = ZLIB_ABCD,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 128), 0, 0, 0,
			INODE_HEADER(2, 0, 0, 0, 0, 1),
			INODE_BASIC_FILE(1024, 0xFFFFFFFF, 0, 4),
			DATA_BLOCK_REF(sizeof((uint8_t[]){ZLIB_ABCD}), 1),
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	uint64_t inode_ref = sqsh_address_ref_create(0, 3);
	rv = sqsh__file_init(&file, &archive, inode_ref);
	ASSERT_EQ(0, rv);

	ASSERT_EQ(SQSH_FILE_TYPE_FILE, (int)sqsh_file_type(&file));
	ASSERT_EQ(false, sqsh_file_has_fragment(&file));

	struct SqshFileReader reader = {0};
	rv = sqsh__file_reader_init(&reader, &file);
	ASSERT_EQ(0, rv);

	rv = sqsh_file_reader_advance(&reader, 0, 4);
	ASSERT_EQ(0, rv);

	size_t size = sqsh_file_reader_size(&reader);
	ASSERT_EQ((size_t)4, size);
	const uint8_t *data = sqsh_file_reader_data(&reader);
	ASSERT_EQ('a', data[0]);
	ASSERT_EQ('b', data[1]);
	ASSERT_EQ('c', data[2]);
	ASSERT_EQ('d', data[3]);

	sqsh__file_reader_cleanup(&reader);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST(file_reader, load_file_from_compressed_data_block_with_offset) {
	int rv;
	struct SqshArchive archive = {0};
	struct SqshFile file = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* datablock */
			[1024] = ZLIB_ABCD,
			/* inode */
			[INODE_TABLE_OFFSET + 256] = METABLOCK_HEADER(0, 128), 0, 0, 0,
			INODE_HEADER(2, 0, 0, 0, 0, 1),
			INODE_BASIC_FILE(1024, 0xFFFFFFFF, 0, 4),
			DATA_BLOCK_REF(sizeof((uint8_t[]){ZLIB_ABCD}), 1),
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	uint64_t inode_ref = sqsh_address_ref_create(256, 3);
	rv = sqsh__file_init(&file, &archive, inode_ref);
	ASSERT_EQ(0, rv);

	ASSERT_EQ(SQSH_FILE_TYPE_FILE, (int)sqsh_file_type(&file));
	ASSERT_EQ(false, sqsh_file_has_fragment(&file));

	struct SqshFileReader reader = {0};
	rv = sqsh__file_reader_init(&reader, &file);
	ASSERT_EQ(0, rv);

	rv = sqsh_file_reader_advance(&reader, 1, 2);
	ASSERT_EQ(0, rv);

	size_t size = sqsh_file_reader_size(&reader);
	ASSERT_EQ((size_t)2, size);
	const uint8_t *data = sqsh_file_reader_data(&reader);
	ASSERT_EQ('b', data[0]);
	ASSERT_EQ('c', data[1]);

	sqsh__file_reader_cleanup(&reader);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST(file_reader, load_file_from_uncompressed_data_block) {
	int rv;
	struct SqshArchive archive = {0};
	struct SqshFile file = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* datablock */
			[1024] = 1, 2, 3, 4, 5,
			/* inode */
			[INODE_TABLE_OFFSET + 256] = METABLOCK_HEADER(0, 128),
			INODE_HEADER(2, 0, 0, 0, 0, 1),
			INODE_BASIC_FILE(1024, 0xFFFFFFFF, 0, 5), DATA_BLOCK_REF(5, 0),
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	uint64_t inode_ref = sqsh_address_ref_create(256, 0);
	rv = sqsh__file_init(&file, &archive, inode_ref);
	ASSERT_EQ(0, rv);

	ASSERT_EQ(SQSH_FILE_TYPE_FILE, (int)sqsh_file_type(&file));
	ASSERT_EQ(false, sqsh_file_has_fragment(&file));

	struct SqshFileReader reader = {0};
	rv = sqsh__file_reader_init(&reader, &file);
	ASSERT_EQ(0, rv);

	rv = sqsh_file_reader_advance(&reader, 0, 5);
	ASSERT_EQ(0, rv);

	size_t size = sqsh_file_reader_size(&reader);
	ASSERT_EQ((size_t)5, size);
	const uint8_t *data = sqsh_file_reader_data(&reader);
	ASSERT_EQ(1, data[0]);
	ASSERT_EQ(2, data[1]);
	ASSERT_EQ(3, data[2]);
	ASSERT_EQ(4, data[3]);
	ASSERT_EQ(5, data[4]);

	sqsh__file_reader_cleanup(&reader);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST(file_reader, skip_over_zero_page) {
	int rv;
	struct SqshArchive archive = {0};
	struct SqshFile file = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* datablock */
			[1024] = 'f', 'o', 'o', 'b', 'a', 'r',
			/* inode */
			[INODE_TABLE_OFFSET + 256] = METABLOCK_HEADER(0, 128),
			INODE_HEADER(2, 0, 0, 0, 0, 1),
			INODE_BASIC_FILE(1024, 0xFFFFFFFF, 0, 32768 + 6),
			DATA_BLOCK_REF(0, 0), /* zero page */
			DATA_BLOCK_REF(6, 0),
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	uint64_t inode_ref = sqsh_address_ref_create(256, 0);
	rv = sqsh__file_init(&file, &archive, inode_ref);
	ASSERT_EQ(0, rv);

	ASSERT_EQ(SQSH_FILE_TYPE_FILE, (int)sqsh_file_type(&file));
	ASSERT_EQ(false, sqsh_file_has_fragment(&file));

	struct SqshFileReader reader = {0};
	rv = sqsh__file_reader_init(&reader, &file);
	ASSERT_EQ(0, rv);

	rv = sqsh_file_reader_advance(&reader, 32768, 6);
	ASSERT_EQ(0, rv);

	size_t size = sqsh_file_reader_size(&reader);
	ASSERT_EQ((size_t)6, size);
	const uint8_t *data = sqsh_file_reader_data(&reader);
	ASSERT_EQ(0, memcmp(data, "foobar", 6));

	sqsh__file_reader_cleanup(&reader);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST_MAIN()
