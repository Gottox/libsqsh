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
 * @file         extract.c
 */

#include "../../include/sqsh_extract_private.h"
#include "../common.h"
#include <lzma.h>
#include <stdint.h>
#include <testlib.h>

static void
api_test_lzma() {
	uint8_t input[] = {0x5d, 0x00, 0x00, 0x80, 0x00, 0xff, 0xff, 0xff, 0xff,
					   0xff, 0xff, 0xff, 0xff, 0x00, 0x30, 0x98, 0x88, 0x98,
					   0x46, 0x7e, 0x1e, 0xb2, 0xff, 0xfa, 0x1c, 0x80, 0x00};
	uint8_t output[1024];

	FILE *test = fopen("/tmp/mytest", "w");
	fwrite(input, sizeof(input), 1, test);
	fclose(test);
	lzma_stream stream = LZMA_STREAM_INIT;
	lzma_ret ret;

	for (size_t i = 0; i < sizeof(input); i++) {
		ret = lzma_alone_decoder(&stream, UINT64_MAX);
		assert(ret == LZMA_OK);

		stream.next_out = output;
		stream.avail_out = sizeof(output);

		stream.next_in = input;
		stream.avail_in = i;
		ret = lzma_code(&stream, LZMA_RUN);
		assert(ret == LZMA_OK);

		stream.next_in = &input[i];
		stream.avail_in = sizeof(input) - i;
		ret = lzma_code(&stream, LZMA_RUN);
		assert(ret == LZMA_STREAM_END);
		assert(memcmp(output, "abcd", 4) == 0);

		ret = lzma_code(&stream, LZMA_FINISH);
		assert(ret == LZMA_STREAM_END);

		lzma_end(&stream);
	}
}

DECLARE_TESTS
TEST(api_test_lzma)
END_TESTS
