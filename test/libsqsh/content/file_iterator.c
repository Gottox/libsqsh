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
 * @file         file_iterator.c
 */

#include "../common.h"
#include <utest.h>

#include <sqsh_archive_private.h>
#include <sqsh_common_private.h>
#include <sqsh_content_private.h>
#include <sqsh_data_private.h>

static const size_t BLOCK_SIZE = 32768;
#define ZERO_BLOCK_SIZE (size_t)16384
uint8_t ZERO_BLOCK[ZERO_BLOCK_SIZE] = {0};

UTEST(file_iterator, load_segment_from_compressed_data_block) {
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

	struct SqshFileIterator iter = {0};
	rv = sqsh__file_iterator_init(&iter, &file);
	ASSERT_EQ(0, rv);

	bool has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	assert(rv > 0);
	ASSERT_EQ(true, has_next);

	size_t size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ((size_t)4, size);
	const uint8_t *data = sqsh_file_iterator_data(&iter);
	ASSERT_EQ('a', data[0]);
	ASSERT_EQ('b', data[1]);
	ASSERT_EQ('c', data[2]);
	ASSERT_EQ('d', data[3]);

	has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(false, has_next);

	size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ((size_t)0, size);
	data = sqsh_file_iterator_data(&iter);
	ASSERT_EQ(NULL, data);

	sqsh__file_iterator_cleanup(&iter);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST(file_iterator, load_two_segments_from_uncompressed_data_block) {
	int rv;
	struct SqshArchive archive = {0};
	struct SqshFile file = {0};
	uint8_t payload[] = {
			/* clang-format off */
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			INODE_HEADER(2, 0, 0, 0, 0, 1),
			INODE_BASIC_FILE(8192, 0xFFFFFFFF, 0, BLOCK_SIZE + 5),
			DATA_BLOCK_REF(1000, 0),
			DATA_BLOCK_REF(5, 0),
			/* datablocks */
			[8192 ... 8192 + 999] = 0xa1,
			[8192 + 1000] = 1, 2, 3, 4, 5
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	uint64_t inode_ref = sqsh_address_ref_create(0, 0);
	rv = sqsh__file_init(&file, &archive, inode_ref);
	ASSERT_EQ(0, rv);

	ASSERT_EQ(SQSH_FILE_TYPE_FILE, (int)sqsh_file_type(&file));
	ASSERT_EQ(false, sqsh_file_has_fragment(&file));

	struct SqshFileIterator iter = {0};
	rv = sqsh__file_iterator_init(&iter, &file);
	ASSERT_EQ(0, rv);

	bool has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	assert(rv > 0);
	ASSERT_EQ(true, has_next);

	size_t size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ((size_t)1000, size);
	const uint8_t *data = sqsh_file_iterator_data(&iter);
	for (size_t i = 0; i < size; i++) {
		ASSERT_EQ(0xa1, data[i]);
	}

	has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	assert(rv > 0);
	ASSERT_EQ(true, has_next);

	size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ(ZERO_BLOCK_SIZE, size);
	data = sqsh_file_iterator_data(&iter);
	ASSERT_EQ(0, memcmp(data, ZERO_BLOCK, size));

	has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	assert(rv > 0);
	ASSERT_EQ(true, has_next);

	size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ(ZERO_BLOCK_SIZE - 1000, size);
	data = sqsh_file_iterator_data(&iter);
	ASSERT_EQ(0, memcmp(data, ZERO_BLOCK, size));

	has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	assert(rv > 0);
	ASSERT_EQ(true, has_next);

	size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ((size_t)5, size);
	data = sqsh_file_iterator_data(&iter);
	ASSERT_EQ(1, data[0]);
	ASSERT_EQ(2, data[1]);
	ASSERT_EQ(3, data[2]);
	ASSERT_EQ(4, data[3]);
	ASSERT_EQ(5, data[4]);

	has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(false, has_next);

	size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ((size_t)0, size);
	data = sqsh_file_iterator_data(&iter);
	ASSERT_EQ(NULL, data);

	sqsh__file_iterator_cleanup(&iter);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST(file_iterator, load_segment_from_uncompressed_data_block) {
	int rv;
	struct SqshArchive archive = {0};
	struct SqshFile file = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* datablock */
			[512] = 1, 2, 3, 4, 5,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			INODE_HEADER(2, 0, 0, 0, 0, 1),
			INODE_BASIC_FILE(512, 0xFFFFFFFF, 0, 5),
			DATA_BLOCK_REF(5, 0),
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	uint64_t inode_ref = sqsh_address_ref_create(0, 0);
	rv = sqsh__file_init(&file, &archive, inode_ref);
	ASSERT_EQ(0, rv);

	ASSERT_EQ(SQSH_FILE_TYPE_FILE, (int)sqsh_file_type(&file));
	ASSERT_EQ(false, sqsh_file_has_fragment(&file));

	struct SqshFileIterator iter = {0};
	rv = sqsh__file_iterator_init(&iter, &file);
	ASSERT_EQ(0, rv);

	bool has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	assert(rv > 0);
	ASSERT_EQ(true, has_next);

	size_t size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ((size_t)5, size);
	const uint8_t *data = sqsh_file_iterator_data(&iter);
	ASSERT_EQ(1, data[0]);
	ASSERT_EQ(2, data[1]);
	ASSERT_EQ(3, data[2]);
	ASSERT_EQ(4, data[3]);
	ASSERT_EQ(5, data[4]);

	has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(false, has_next);

	size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ((size_t)0, size);
	data = sqsh_file_iterator_data(&iter);
	ASSERT_EQ(NULL, data);

	sqsh__file_iterator_cleanup(&iter);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST(file_iterator, load_zero_padding) {
	int rv;
	struct SqshArchive archive = {0};
	struct SqshFile file = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			INODE_HEADER(2, 0, 0, 0, 0, 1),
			INODE_BASIC_FILE(512, 0xFFFFFFFF, 0, BLOCK_SIZE + 5),
			DATA_BLOCK_REF(0, 1),
			DATA_BLOCK_REF(0, 1),
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	uint64_t inode_ref = sqsh_address_ref_create(0, 0);
	rv = sqsh__file_init(&file, &archive, inode_ref);
	ASSERT_EQ(0, rv);

	ASSERT_EQ(SQSH_FILE_TYPE_FILE, (int)sqsh_file_type(&file));
	ASSERT_EQ(false, sqsh_file_has_fragment(&file));

	struct SqshFileIterator iter = {0};
	rv = sqsh__file_iterator_init(&iter, &file);
	ASSERT_EQ(0, rv);

	bool has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	assert(rv > 0);
	ASSERT_EQ(true, has_next);
	size_t size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ(ZERO_BLOCK_SIZE, size);

	has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	assert(rv > 0);
	ASSERT_EQ(true, has_next);
	size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ(ZERO_BLOCK_SIZE, size);

	has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	assert(rv > 0);
	ASSERT_EQ(true, has_next);

	size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ((size_t)5, size);
	const uint8_t *data = sqsh_file_iterator_data(&iter);
	ASSERT_EQ(0, data[0]);
	ASSERT_EQ(0, data[1]);
	ASSERT_EQ(0, data[2]);
	ASSERT_EQ(0, data[3]);
	ASSERT_EQ(0, data[4]);

	has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(false, has_next);

	size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ((size_t)0, size);
	data = sqsh_file_iterator_data(&iter);
	ASSERT_EQ(NULL, data);

	sqsh__file_iterator_cleanup(&iter);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST(file_iterator, load_zero_big_padding) {
	int rv;
	struct SqshArchive archive = {0};
	struct SqshFile file = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			INODE_HEADER(2, 0, 0, 0, 0, 1),
			INODE_BASIC_FILE(512, 0xFFFFFFFF, 0, BLOCK_SIZE + 5),
			DATA_BLOCK_REF(0, 1),
			DATA_BLOCK_REF(0, 1),
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	uint64_t inode_ref = sqsh_address_ref_create(0, 0);
	rv = sqsh__file_init(&file, &archive, inode_ref);
	ASSERT_EQ(0, rv);

	ASSERT_EQ(SQSH_FILE_TYPE_FILE, (int)sqsh_file_type(&file));
	ASSERT_EQ(false, sqsh_file_has_fragment(&file));

	struct SqshFileIterator iter = {0};
	rv = sqsh__file_iterator_init(&iter, &file);
	ASSERT_EQ(0, rv);

	bool has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	assert(rv > 0);
	ASSERT_EQ(true, has_next);
	size_t size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ(ZERO_BLOCK_SIZE, size);

	has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	assert(rv > 0);
	ASSERT_EQ(true, has_next);
	size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ(ZERO_BLOCK_SIZE, size);

	has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	assert(rv > 0);
	ASSERT_EQ(true, has_next);

	size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ((size_t)5, size);
	const uint8_t *data = sqsh_file_iterator_data(&iter);
	ASSERT_EQ(0, data[0]);
	ASSERT_EQ(0, data[1]);
	ASSERT_EQ(0, data[2]);
	ASSERT_EQ(0, data[3]);
	ASSERT_EQ(0, data[4]);

	has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(false, has_next);

	size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ((size_t)0, size);
	data = sqsh_file_iterator_data(&iter);
	ASSERT_EQ(NULL, data);

	sqsh__file_iterator_cleanup(&iter);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST(file_iterator, load_zero_block) {
	int rv;
	struct SqshArchive archive = {0};
	struct SqshFile file = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			INODE_HEADER(2, 0, 0, 0, 0, 1),
			INODE_BASIC_FILE(512, 0xFFFFFFFF, 0, ZERO_BLOCK_SIZE),
			DATA_BLOCK_REF(0, 0),
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	uint64_t inode_ref = sqsh_address_ref_create(0, 0);
	rv = sqsh__file_init(&file, &archive, inode_ref);
	ASSERT_EQ(0, rv);

	ASSERT_EQ(SQSH_FILE_TYPE_FILE, (int)sqsh_file_type(&file));
	ASSERT_EQ(false, sqsh_file_has_fragment(&file));

	struct SqshFileIterator iter = {0};
	rv = sqsh__file_iterator_init(&iter, &file);
	ASSERT_EQ(0, rv);

	bool has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	assert(rv > 0);
	ASSERT_EQ(true, has_next);

	size_t size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ(ZERO_BLOCK_SIZE, size);

	const uint8_t *data = sqsh_file_iterator_data(&iter);
	ASSERT_EQ(0, memcmp(data, ZERO_BLOCK, size));

	has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(false, has_next);

	sqsh__file_iterator_cleanup(&iter);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST(file_iterator, load_two_zero_blocks) {
	int rv;
	struct SqshArchive archive = {0};
	struct SqshFile file = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			INODE_HEADER(2, 0, 0, 0, 0, 1),
			INODE_BASIC_FILE(512, 0xFFFFFFFF, 0, ZERO_BLOCK_SIZE * 2),
			DATA_BLOCK_REF(0, 0),
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	uint64_t inode_ref = sqsh_address_ref_create(0, 0);
	rv = sqsh__file_init(&file, &archive, inode_ref);
	ASSERT_EQ(0, rv);

	ASSERT_EQ(SQSH_FILE_TYPE_FILE, (int)sqsh_file_type(&file));
	ASSERT_EQ(false, sqsh_file_has_fragment(&file));

	struct SqshFileIterator iter = {0};
	rv = sqsh__file_iterator_init(&iter, &file);
	ASSERT_EQ(0, rv);

	bool has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	assert(rv > 0);
	ASSERT_EQ(true, has_next);

	size_t size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ(ZERO_BLOCK_SIZE, size);

	const uint8_t *data = sqsh_file_iterator_data(&iter);
	ASSERT_EQ(0, memcmp(data, ZERO_BLOCK, size));

	has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	assert(rv > 0);
	ASSERT_EQ(true, has_next);

	size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ(ZERO_BLOCK_SIZE, size);

	ASSERT_EQ(0, memcmp(data, ZERO_BLOCK, size));

	has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(false, has_next);

	sqsh__file_iterator_cleanup(&iter);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST(file_iterator, load_two_sparse_blocks) {
	int rv;
	struct SqshArchive archive = {0};
	struct SqshFile file = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			INODE_HEADER(2, 0, 0, 0, 0, 1),
			INODE_BASIC_FILE(512, 0xFFFFFFFF, 0, BLOCK_SIZE * 2),
			DATA_BLOCK_REF(0, 0),
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	uint64_t inode_ref = sqsh_address_ref_create(0, 0);
	rv = sqsh__file_init(&file, &archive, inode_ref);
	ASSERT_EQ(0, rv);

	ASSERT_EQ(SQSH_FILE_TYPE_FILE, (int)sqsh_file_type(&file));
	ASSERT_EQ(false, sqsh_file_has_fragment(&file));

	struct SqshFileIterator iter = {0};
	rv = sqsh__file_iterator_init(&iter, &file);
	ASSERT_EQ(0, rv);

	bool has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	assert(rv > 0);
	ASSERT_EQ(true, has_next);

	size_t size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ(ZERO_BLOCK_SIZE, size);

	const uint8_t *data = sqsh_file_iterator_data(&iter);
	ASSERT_EQ(0, memcmp(data, ZERO_BLOCK, size));

	has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	assert(rv > 0);
	ASSERT_EQ(true, has_next);

	size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ(ZERO_BLOCK_SIZE, size);

	ASSERT_EQ(0, memcmp(data, ZERO_BLOCK, size));

	has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	assert(rv > 0);
	ASSERT_EQ(true, has_next);

	size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ(ZERO_BLOCK_SIZE, size);

	ASSERT_EQ(0, memcmp(data, ZERO_BLOCK, size));

	has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	assert(rv > 0);
	ASSERT_EQ(true, has_next);

	size = sqsh_file_iterator_size(&iter);
	ASSERT_EQ(ZERO_BLOCK_SIZE, size);

	ASSERT_EQ(0, memcmp(data, ZERO_BLOCK, size));

	has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(false, has_next);

	sqsh__file_iterator_cleanup(&iter);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST(file_iterator, open_directory_with_file_iterator) {
	int rv;
	struct SqshArchive archive = {0};
	struct SqshFile file = {0};
	uint8_t payload[8192] = {
			/* clang-format off */
			SQSH_HEADER,
			/* inode */
			[INODE_TABLE_OFFSET] = METABLOCK_HEADER(0, 128),
			INODE_HEADER(1, 0, 0, 0, 0, 1),
			INODE_BASIC_DIR(512, 104, 0, 0),
			/* clang-format on */
	};
	mk_stub(&archive, payload, sizeof(payload));

	uint64_t inode_ref = sqsh_address_ref_create(0, 0);
	rv = sqsh__file_init(&file, &archive, inode_ref);
	ASSERT_EQ(0, rv);

	ASSERT_EQ(SQSH_FILE_TYPE_DIRECTORY, (int)sqsh_file_type(&file));

	struct SqshFileIterator iter = {0};
	rv = sqsh__file_iterator_init(&iter, &file);
	ASSERT_EQ(-SQSH_ERROR_NOT_A_FILE, rv);

	sqsh__file_iterator_cleanup(&iter);
	sqsh__file_cleanup(&file);
	sqsh__archive_cleanup(&archive);
}

UTEST_MAIN()
