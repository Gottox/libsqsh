/******************************************************************************
 *                                                                            *
 * Copyright (c) 2023-2024, Enno Boland <g@s01.de>                            *
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
 * @file         extract_manager.c
 */

#include <sqsh_extract_private.h>

#include <sqsh_archive.h>
#include <sqsh_common_private.h>
#include <sqsh_error.h>

#include <cextras/collection.h>
#include <sqsh_mapper.h>
#include <sqsh_mapper_private.h>

static void
buffer_cleanup(void *buffer) {
	struct SqshExtractBuffer *extract_buffer = buffer;
	cx_buffer_cleanup(&extract_buffer->buffer);
}

SQSH_NO_UNUSED int
sqsh__extract_manager_init(
		struct SqshExtractManager *manager, struct SqshArchive *archive,
		uint32_t block_size, size_t size) {
	int rv;
	const struct SqshConfig *config = sqsh_archive_config(archive);
	const size_t lru_size =
			SQSH_CONFIG_DEFAULT(config->compression_lru_size, 128);
	const struct SqshSuperblock *superblock = sqsh_archive_superblock(archive);
	enum SqshSuperblockCompressionId compression_id =
			sqsh_superblock_compression_id(superblock);

	manager->extractor_impl = sqsh__extractor_impl_from_id(compression_id);
	if (manager->extractor_impl == NULL) {
		return -SQSH_ERROR_COMPRESSION_UNSUPPORTED;
	}

	if (size == 0) {
		return -SQSH_ERROR_SIZE_MISMATCH;
	}

	/* Give a bit of room to avoid too many key hash collisions */

	rv = sqsh__mutex_init(&manager->lock);
	if (rv < 0) {
		goto out;
	}
	rv = cx_rc_radix_tree_init(
			&manager->radix_tree, sizeof(struct SqshExtractBuffer),
			buffer_cleanup);
	if (rv < 0) {
		goto out;
	}
	rv = cx_lru_init(
			&manager->lru, lru_size, &cx_lru_rc_radix_tree,
			&manager->radix_tree);
	if (rv < 0) {
		goto out;
	}
	manager->map_manager = sqsh_archive_map_manager(archive);

	manager->block_size = block_size;

out:
	if (rv < 0) {
		sqsh__extract_manager_cleanup(manager);
	}
	return rv;
}

int
sqsh__extract_manager_uncompress(
		struct SqshExtractManager *manager, const struct SqshMapReader *reader,
		const struct SqshExtractBuffer **target) {
	int rv = 0;
	struct SqshExtractor extractor = {0};
	const struct SqshExtractorImpl *extractor_impl = manager->extractor_impl;
	const uint32_t block_size = manager->block_size;

	rv = sqsh__mutex_lock(&manager->lock);
	if (rv < 0) {
		goto out;
	}

	const uint64_t address = sqsh__map_reader_address(reader);
	const size_t size = sqsh__map_reader_size(reader);

	*target = cx_rc_radix_tree_retain(&manager->radix_tree, address);

	if (*target == NULL) {
		struct SqshExtractBuffer buffer = {.key = address};
		rv = cx_buffer_init(&buffer.buffer);
		if (rv < 0) {
			goto out;
		}
		const uint8_t *data = sqsh__map_reader_data(reader);

		rv = sqsh__extractor_init(
				&extractor, &buffer.buffer, extractor_impl, block_size);
		if (rv < 0) {
			goto out;
		}

		rv = sqsh__extractor_write(&extractor, data, size);
		if (rv < 0) {
			cx_buffer_cleanup(&buffer.buffer);
			goto out;
		}

		rv = sqsh__extractor_finish(&extractor);
		if (rv < 0) {
			cx_buffer_cleanup(&buffer.buffer);
			goto out;
		}

		*target = cx_rc_radix_tree_put(&manager->radix_tree, address, &buffer);
	}
	rv = cx_lru_touch(&manager->lru, address);

out:
	sqsh__extractor_cleanup(&extractor);
	sqsh__mutex_unlock(&manager->lock);
	return rv;
}

int
sqsh__extract_manager_release(
		struct SqshExtractManager *manager,
		const struct SqshExtractBuffer *buffer) {
	int rv = sqsh__mutex_lock(&manager->lock);
	if (rv < 0) {
		goto out;
	}

	rv = cx_rc_radix_tree_release(&manager->radix_tree, buffer->key);

	sqsh__mutex_unlock(&manager->lock);
out:
	return rv;
}

int
sqsh__extract_manager_cleanup(struct SqshExtractManager *manager) {
	cx_lru_cleanup(&manager->lru);
	cx_rc_radix_tree_cleanup(&manager->radix_tree);
	sqsh__mutex_destroy(&manager->lock);

	return 0;
}
