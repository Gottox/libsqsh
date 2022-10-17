/******************************************************************************
 *                                                                            *
 * Copyright (c) 2022, Enno Boland <g@s01.de>                                 *
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
 * @author       Enno Boland (mail@eboland.de)
 * @file         xattr_internal.h
 */

#include "xattr.h"
#include <stdint.h>

#ifndef SQSH_XATTR_INTERNAL_H

#define SQSH_XATTR_INTERNAL_H

struct SQSH_UNALIGNED SqshXattrKey {
	uint16_t type;
	uint16_t name_size;
	// uint8_t name[0]; // [name_size - strlen(prefix)];
};
STATIC_ASSERT(sizeof(struct SqshXattrKey) == SQSH_SIZEOF_XATTR_KEY);

struct SQSH_UNALIGNED SqshXattrValue {
	uint32_t value_size;
	// uint8_t value[0]; // [value_size]
};
STATIC_ASSERT(sizeof(struct SqshXattrValue) == SQSH_SIZEOF_XATTR_VALUE);

struct SQSH_UNALIGNED SqshXattrLookupTable {
	uint64_t xattr_ref;
	uint32_t count;
	uint32_t size;
};
STATIC_ASSERT(
		sizeof(struct SqshXattrLookupTable) == SQSH_SIZEOF_XATTR_LOOKUP_TABLE);

struct SQSH_UNALIGNED SqshXattrIdTable {
	uint64_t xattr_table_start;
	uint32_t xattr_ids;
	uint32_t _unused;
	// uint64_t table[0]; // [ceil(xattr_ids / 512.0)]
};
STATIC_ASSERT(sizeof(struct SqshXattrIdTable) == SQSH_SIZEOF_XATTR_ID_TABLE);

#endif /* end of include guard SQSH_XATTR_INTERNAL_H */
