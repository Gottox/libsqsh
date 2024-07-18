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

/**
 * Calculates pow(x,y) % mod
 */
static uint64_t
mod_power(uint64_t x, uint64_t y, uint64_t mod) {
	uint64_t res = 1;

	for (; y; y = y >> 1) {
		if (y & 1) {
			res = (res * x) % mod;
		}

		x = (x * x) % mod;
	}

	return res;
}

static bool
maybe_prime(uint64_t n) {
	static const uint64_t a = 2;

	return mod_power(a, n - 1, n) == 1;
}

static uint64_t
find_next_maybe_prime(size_t n) {
	for (; maybe_prime(n) == false; n++) {
	}

	return n;
}

static void
buffer_cleanup(void *buffer) {
	cx_buffer_cleanup(buffer);
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
	size = find_next_maybe_prime(2 * size);

	rv = sqsh__mutex_init(&manager->lock);
	if (rv < 0) {
		goto out;
	}
	rv = cx_rc_hash_map_init(
			&manager->hash_map, size, sizeof(struct CxBuffer), buffer_cleanup);
	if (rv < 0) {
		goto out;
	}
	rv = cx_lru_init(
			&manager->lru, lru_size, &cx_lru_rc_hash_map, &manager->hash_map);
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
do_uncompress(
		struct SqshExtractManager *manager, struct SqshMapReader *reader,
		size_t size, const struct CxBuffer **target) {
	const uint32_t block_size = manager->block_size;
	const uint64_t address = sqsh__map_reader_address(reader);

	struct SqshExtractor extractor = {0};
	const struct SqshExtractorImpl *extractor_impl = manager->extractor_impl;
	int rv = 0;
	struct CxBuffer buffer = {0};
	rv = cx_buffer_init(&buffer);
	if (rv < 0) {
		goto out;
	}

	rv = sqsh__extractor_init(&extractor, &buffer, extractor_impl, block_size);
	if (rv < 0) {
		goto out;
	}

	size_t remaining_size = size;
	while (remaining_size > 0) {
		const size_t current_size = SQSH_MIN(
				sqsh__map_reader_remaining_direct(reader), remaining_size);

		rv = sqsh__map_reader_advance(reader, 0, current_size);
		if (rv < 0) {
			goto out;
		}

		const uint8_t *data = sqsh__map_reader_data(reader);
		rv = sqsh__extractor_write(&extractor, data, current_size);
		if (rv < 0) {
			goto out;
		}

		rv = sqsh__map_reader_advance(reader, current_size, 0);
		if (rv < 0) {
			goto out;
		}

		remaining_size -= current_size;
	}
	rv = sqsh__extractor_finish(&extractor);
	if (rv < 0) {
		goto out;
	}

	*target = cx_rc_hash_map_put(&manager->hash_map, address, &buffer);
out:
	if (rv < 0) {
		cx_buffer_cleanup(&buffer);
	}
	sqsh__extractor_cleanup(&extractor);
	return rv;
}

int
sqsh__extract_manager_uncompress2(
		struct SqshExtractManager *manager, struct SqshMapReader *reader,
		size_t size, const struct CxBuffer **target) {
	int rv = 0;

	rv = sqsh__mutex_lock(&manager->lock);
	if (rv < 0) {
		goto out;
	}

	const uint64_t address = sqsh__map_reader_address(reader);

	rv = sqsh__map_reader_advance(reader, 0, size);

	*target = cx_rc_hash_map_retain(&manager->hash_map, address);

	if (*target == NULL) {
		rv = do_uncompress(manager, reader, size, target);
		if (rv < 0) {
			goto out;
		}
	}
	rv = cx_lru_touch(&manager->lru, address);

out:
	sqsh__mutex_unlock(&manager->lock);
	return rv;
}
int
sqsh__extract_manager_uncompress(
		struct SqshExtractManager *manager, const struct SqshMapReader *reader,
		const struct CxBuffer **target) {
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

	*target = cx_rc_hash_map_retain(&manager->hash_map, address);

	if (*target == NULL) {
		struct CxBuffer buffer = {0};
		rv = cx_buffer_init(&buffer);
		if (rv < 0) {
			goto out;
		}
		const uint8_t *data = sqsh__map_reader_data(reader);

		rv = sqsh__extractor_init(
				&extractor, &buffer, extractor_impl, block_size);
		if (rv < 0) {
			goto out;
		}

		rv = sqsh__extractor_write(&extractor, data, size);
		if (rv < 0) {
			cx_buffer_cleanup(&buffer);
			goto out;
		}

		rv = sqsh__extractor_finish(&extractor);
		if (rv < 0) {
			cx_buffer_cleanup(&buffer);
			goto out;
		}

		*target = cx_rc_hash_map_put(&manager->hash_map, address, &buffer);
	}
	rv = cx_lru_touch(&manager->lru, address);

out:
	sqsh__extractor_cleanup(&extractor);
	sqsh__mutex_unlock(&manager->lock);
	return rv;
}

int
sqsh__extract_manager_release(
		struct SqshExtractManager *manager, const struct CxBuffer *buffer) {
	int rv = sqsh__mutex_lock(&manager->lock);
	if (rv < 0) {
		goto out;
	}

	rv = cx_rc_hash_map_release(&manager->hash_map, buffer);

	sqsh__mutex_unlock(&manager->lock);
out:
	return rv;
}

int
sqsh__extract_manager_cleanup(struct SqshExtractManager *manager) {
	cx_lru_cleanup(&manager->lru);
	cx_rc_hash_map_cleanup(&manager->hash_map);
	sqsh__mutex_destroy(&manager->lock);

	return 0;
}
