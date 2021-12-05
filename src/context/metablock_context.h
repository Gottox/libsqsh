/******************************************************************************
 *                                                                            *
 * Copyright (c) 2021, Enno Boland <g@s01.de>                                 *
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
 * @author      : Enno Boland (mail@eboland.de)
 * @file        : metablock_context
 * @created     : Saturday Dec 04, 2021 14:54:28 CET
 */

#include "../mapper/memory_mapper.h"
#include "../utils.h"
#include <stdint.h>

#ifndef METABLOCK_CONTEXT_H

#define METABLOCK_CONTEXT_H

#define HSQS_METABLOCK_BLOCK_SIZE 8192

struct HsqsSuperblockContext;
struct HsqsBuffer;

struct HsqsMetablockContext {
	struct HsqsMemoryMap map;
};

HSQS_NO_UNUSED int hsqs_metablock_init(
		struct HsqsMetablockContext *context,
		const struct HsqsSuperblockContext *superblock, uint64_t address);

uint32_t
hsqs_metablock_compressed_size(const struct HsqsMetablockContext *context);

HSQS_NO_UNUSED int hsqs_metablock_to_buffer(
		struct HsqsMetablockContext *context, struct HsqsBuffer *buffer);

int hsqs_metablock_cleanup(struct HsqsMetablockContext *context);

#endif /* end of include guard METABLOCK_CONTEXT_H */
