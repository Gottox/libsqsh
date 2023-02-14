/******************************************************************************
 *                                                                            *
 * Copyright (c) 2023, Enno Boland <g@s01.de>                                 *
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
 * @file         cursor.c
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/sqsh_error.h"
#include "../../include/sqsh_mapper.h"
#include "src/utils.h"

int
sqsh__map_cursor_init(
		struct SqshMapCursor *cursor, struct SqshMapper *mapper,
		const uint64_t start_address, const uint64_t upper_limit) {
	cursor->mapper = mapper;
	cursor->offset = 0;
	cursor->size = 0;
	cursor->upper_limit = upper_limit;
	return sqsh_mapper_map(&cursor->mapping, mapper, start_address, 4096);
}

int
sqsh__map_cursor_advance(
		struct SqshMapCursor *cursor, sqsh_index_t offset, size_t size) {
	cursor->offset += offset;
	size_t new_size = cursor->offset + size;
	if (SQSH_ADD_OVERFLOW(cursor->size, size, &new_size)) {
		return SQSH_ERROR_INTEGER_OVERFLOW;
	}
	if (new_size > cursor->upper_limit) {
		return SQSH_ERROR_INTEGER_OVERFLOW;
	}
	return sqsh_mapping_resize(&cursor->mapping, new_size);
}

const uint8_t *
sqsh__map_cursor_data(struct SqshMapCursor *cursor) {
	return &sqsh_mapping_data(&cursor->mapping)[cursor->offset];
}

int
sqsh__map_cursor_cleanup(struct SqshMapCursor *cursor) {
	sqsh_mapping_unmap(&cursor->mapping);
	return 0;
}
