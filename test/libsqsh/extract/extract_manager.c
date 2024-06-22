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
 * @file         extract_manager.c
 */

#include "../common.h"
#include <utest.h>

#include <sqsh_archive_private.h>
#include <sqsh_data_private.h>
#include <sqsh_extract_private.h>
#include <sqsh_mapper_private.h>

UTEST(directory_iterator, decompress) {
	int rv;
	struct SqshArchive archive = {0};
	struct SqshExtractManager manager = {0};
	const struct SqshExtractBuffer *buffer = NULL;
	uint8_t payload[8192] = {SQSH_HEADER, ZLIB_ABCD};

	mk_stub(&archive, payload, sizeof(payload));

	struct SqshMapManager *map_manager = sqsh_archive_map_manager(&archive);
	struct SqshMapReader reader = {0};
	rv = sqsh__map_reader_init(
			&reader, map_manager, sizeof(struct SqshDataSuperblock),
			sizeof(payload));
	ASSERT_EQ(0, rv);

	rv = sqsh__map_reader_advance(&reader, 0, CHUNK_SIZE(ZLIB_ABCD));
	ASSERT_EQ(0, rv);

	rv = sqsh__extract_manager_init(&manager, &archive, 8192, 10);
	ASSERT_EQ(0, rv);

	rv = sqsh__extract_manager_uncompress(&manager, &reader, &buffer);
	ASSERT_EQ(0, rv);
	ASSERT_NE(NULL, buffer);
	ASSERT_EQ((size_t)4, cx_buffer_size(&buffer->buffer));
	ASSERT_EQ(0, memcmp(cx_buffer_data(&buffer->buffer), "abcd", 4));

	sqsh__map_reader_cleanup(&reader);
	sqsh__extract_manager_release(&manager, buffer);
	sqsh__extract_manager_cleanup(&manager);
	sqsh__archive_cleanup(&archive);
}

UTEST(directory_iterator, decompress_and_cached) {
	int rv;
	struct SqshArchive archive = {0};
	struct SqshExtractManager manager = {0};
	const struct SqshExtractBuffer *buffer = NULL;
	const struct SqshExtractBuffer *cached_buffer = NULL;
	uint8_t payload[8192] = {SQSH_HEADER, ZLIB_ABCD};

	mk_stub(&archive, payload, sizeof(payload));

	struct SqshMapManager *map_manager = sqsh_archive_map_manager(&archive);
	struct SqshMapReader reader = {0};
	rv = sqsh__map_reader_init(
			&reader, map_manager, sizeof(struct SqshDataSuperblock),
			sizeof(payload));
	ASSERT_EQ(0, rv);

	rv = sqsh__map_reader_advance(&reader, 0, CHUNK_SIZE(ZLIB_ABCD));
	ASSERT_EQ(0, rv);

	rv = sqsh__extract_manager_init(&manager, &archive, 8192, 10);
	ASSERT_EQ(0, rv);

	rv = sqsh__extract_manager_uncompress(&manager, &reader, &buffer);
	ASSERT_EQ(0, rv);
	ASSERT_NE(NULL, buffer);
	ASSERT_EQ((size_t)4, cx_buffer_size(&buffer->buffer));
	ASSERT_EQ(0, memcmp(cx_buffer_data(&buffer->buffer), "abcd", 4));

	rv = sqsh__extract_manager_uncompress(&manager, &reader, &cached_buffer);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(cached_buffer, buffer);

	sqsh__map_reader_cleanup(&reader);
	sqsh__extract_manager_release(&manager, buffer);
	sqsh__extract_manager_release(&manager, cached_buffer);
	sqsh__extract_manager_cleanup(&manager);
	sqsh__archive_cleanup(&archive);
}

UTEST_MAIN()
