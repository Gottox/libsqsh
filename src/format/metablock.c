/**
 * @author      : Enno Boland (mail@eboland.de)
 * @file        : metablock
 * @created     : Friday Apr 30, 2021 21:39:44 CEST
 */

#include "metablock.h"
#include "../squash.h"
#include "../utils.h"
#include "superblock.h"

#include <assert.h>
#include <stdint.h>

struct SquashMetablock {
	uint16_t header;
	// uint8_t data[0];
};

int
squash_format_metablock_is_compressed(const struct SquashMetablock *metablock) {
	return !(metablock->header & 0x8000);
}

const uint8_t *
squash_format_metablock_data(const struct SquashMetablock *metablock) {
	const uint8_t *tmp = (uint8_t *)metablock;
	return (uint8_t *)&tmp[sizeof(struct SquashMetablock)];
}

size_t
squash_format_metablock_size(const struct SquashMetablock *metablock) {
	return metablock->header & 0x7FFF;
}
