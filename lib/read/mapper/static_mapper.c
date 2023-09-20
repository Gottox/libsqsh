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
 * @file         static_mapper.c
 */

#include "../../../include/sqsh_mapper_private.h"

static int
sqsh_mapper_static_mem_init(
		struct SqshMapper *mapper, const void *input, size_t *size,
		const struct SqshConfig *config) {
	(void)size;
	(void)config;
	mapper->data.sm.data = input;
	return 0;
}
static int
sqsh_mapper_static_mem_map(struct SqshMapSlice *mapping) {
	size_t offset = mapping->offset;
	/* Cast to remove const qualifier. */
	uint8_t *data = (uint8_t *)mapping->mapper->data.sm.data;
	mapping->data = &data[offset];
	return 0;
}
static int
sqsh_mapper_static_mem_cleanup(struct SqshMapper *mapper) {
	(void)mapper;
	return 0;
}
static int
sqsh_mapping_static_mem_unmap(struct SqshMapSlice *mapping) {
	mapping->data = NULL;
	return 0;
}
static const uint8_t *
sqsh_mapping_static_mem_data(const struct SqshMapSlice *mapping) {
	return mapping->data;
}

static const struct SqshMemoryMapperImpl impl = {
		.block_size_hint = SIZE_MAX,
		.init = sqsh_mapper_static_mem_init,
		.map = sqsh_mapper_static_mem_map,
		.cleanup = sqsh_mapper_static_mem_cleanup,
		.map_data = sqsh_mapping_static_mem_data,
		.unmap = sqsh_mapping_static_mem_unmap,
};
const struct SqshMemoryMapperImpl *const sqsh_mapper_impl_static = &impl;
