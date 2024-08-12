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
 * @file         xattr_iterator.c
 */

#include <sqsh_xattr_private.h>

#include <cextras/memory.h>
#include <sqsh_archive.h>
#include <sqsh_common_private.h>
#include <sqsh_data_private.h>
#include <sqsh_error.h>
#include <sqsh_file_private.h>

int
sqsh__xattr_iterator_init(
		struct SqshXattrIterator *iterator, const struct SqshFile *file) {
	int rv;
	struct SqshDataXattrLookupTable ref = {0};
	struct SqshXattrTable *xattr_table = NULL;
	struct SqshArchive *sqsh = file->archive;
	const struct SqshSuperblock *superblock = sqsh_archive_superblock(sqsh);
	uint32_t index = sqsh_file_xattr_index(file);

	if (index == SQSH_INODE_NO_XATTR ||
		sqsh_superblock_has_xattr_table(superblock) == false) {
		iterator->remaining_entries = 0;
		return 0;
	}

	rv = sqsh_archive_xattr_table(sqsh, &xattr_table);
	if (rv < 0) {
		return rv;
	}

	if (index != SQSH_INODE_NO_XATTR && xattr_table == NULL) {
		return -SQSH_ERROR_CORRUPTED_INODE;
	}

	rv = sqsh__xattr_table_get(xattr_table, index, &ref);
	if (rv < 0) {
		goto out;
	}

	/* The XATTR table is the last block in the file system. */
	const uint64_t address_ref = sqsh__data_xattr_lookup_table_xattr_ref(&ref);
	const uint64_t inner_offset = sqsh_address_ref_inner_offset(address_ref);
	const uint64_t outer_offset = sqsh_address_ref_outer_offset(address_ref);
	const uint64_t archive_size = sqsh_superblock_bytes_used(superblock);

	uint64_t start_block = sqsh__xattr_table_start(xattr_table);
	if (SQSH_ADD_OVERFLOW(start_block, outer_offset, &start_block)) {
		rv = -SQSH_ERROR_INTEGER_OVERFLOW;
		goto out;
	}

	rv = sqsh__metablock_reader_init(
			&iterator->metablock, sqsh, start_block, archive_size);
	if (rv < 0) {
		goto out;
	}

	iterator->remaining_entries = sqsh__data_xattr_lookup_table_count(&ref);
	iterator->remaining_size = sqsh__data_xattr_lookup_table_size(&ref);
	iterator->next_offset = inner_offset;
	iterator->value_index = 0;
	iterator->context = xattr_table;
	iterator->archive = sqsh;
	iterator->upper_limit = archive_size;

out:
	if (rv < 0) {
		sqsh__xattr_iterator_cleanup(iterator);
	}
	return rv;
}

struct SqshXattrIterator *
sqsh_xattr_iterator_new(const struct SqshFile *file, int *err) {
	SQSH_NEW_IMPL(sqsh__xattr_iterator_init, struct SqshXattrIterator, file);
}

static const struct SqshDataXattrValue *
get_value(const struct SqshXattrIterator *iterator) {
	const struct SqshMetablockReader *source;

	if (iterator->value_index == 0) {
		source = &iterator->out_of_line_value;
	} else {
		source = &iterator->metablock;
	}

	const uint8_t *data = sqsh__metablock_reader_data(source);
	return (const struct SqshDataXattrValue *)&data[iterator->value_index];
}

static const struct SqshDataXattrKey *
get_key(const struct SqshXattrIterator *iterator) {
	const uint8_t *data = sqsh__metablock_reader_data(&iterator->metablock);

	return (const struct SqshDataXattrKey *)data;
}

static int
xattr_value_indirect_load(struct SqshXattrIterator *iterator) {
	const struct SqshDataXattrValue *value = get_value(iterator);
	int rv = 0;
	if (sqsh__data_xattr_value_size(value) != 8) {
		return -SQSH_ERROR_SIZE_MISMATCH;
	}
	uint64_t ref = sqsh__data_xattr_value_ref(value);
	uint64_t outer_offset = sqsh_address_ref_outer_offset(ref);
	uint16_t inner_offset = sqsh_address_ref_inner_offset(ref);

	uint64_t start_block = sqsh__xattr_table_start(iterator->context);
	size_t size = sizeof(struct SqshDataXattrValue);
	if (SQSH_ADD_OVERFLOW(start_block, outer_offset, &start_block)) {
		return -SQSH_ERROR_INTEGER_OVERFLOW;
	}
	rv = sqsh__metablock_reader_init(
			&iterator->out_of_line_value, iterator->archive, start_block,
			iterator->upper_limit);
	if (rv < 0) {
		goto out;
	}
	rv = sqsh__metablock_reader_advance(
			&iterator->out_of_line_value, inner_offset, size);
	if (rv < 0) {
		goto out;
	}

	/* Value offset 0 marks an indirect load. */
	iterator->value_index = 0;
	value = get_value(iterator);
	size += sqsh__data_xattr_value_size(value);
	rv = sqsh__metablock_reader_advance(&iterator->out_of_line_value, 0, size);
	if (rv < 0) {
		goto out;
	}

out:
	return rv;
}

bool
sqsh_xattr_iterator_next(struct SqshXattrIterator *iterator, int *err) {
	int rv = 0;
	size_t size =
			sizeof(struct SqshDataXattrKey) + sizeof(struct SqshDataXattrValue);
	bool has_next = false;

	sqsh__metablock_reader_cleanup(&iterator->out_of_line_value);

	if (iterator->remaining_entries == 0) {
		if (iterator->remaining_size != 0) {
			rv = -SQSH_ERROR_XATTR_SIZE_MISMATCH;
		}
		goto out;
	}

	/* Load Key Header */
	rv = sqsh__metablock_reader_advance(
			&iterator->metablock, iterator->next_offset,
			sizeof(struct SqshDataXattrKey));
	if (rv < 0) {
		goto out;
	}

	/* Load Key Name */
	const uint16_t name_size = sqsh_xattr_iterator_name_size(iterator);
	size += name_size;
	rv = sqsh__metablock_reader_advance(&iterator->metablock, 0, size);
	if (rv < 0) {
		goto out;
	}

	/* Load Value Header */
	iterator->value_index = sizeof(struct SqshDataXattrKey) + name_size;

	/* Load Value */
	const uint32_t value_size = sqsh_xattr_iterator_value_size2(iterator);
	size += value_size;
	rv = sqsh__metablock_reader_advance(&iterator->metablock, 0, size);
	if (rv < 0) {
		goto out;
	}

	iterator->next_offset = size;

	if (sqsh_xattr_iterator_is_indirect(iterator)) {
		rv = xattr_value_indirect_load(iterator);
		if (rv < 0) {
			goto out;
		}
	}

	const size_t processed_size = sqsh_xattr_iterator_prefix_size(iterator) +
			name_size + sqsh_xattr_iterator_value_size2(iterator) + 1;
	if (SQSH_SUB_OVERFLOW(
				iterator->remaining_size, processed_size,
				&iterator->remaining_size)) {
		rv = -SQSH_ERROR_XATTR_SIZE_MISMATCH;
		goto out;
	}

	iterator->remaining_entries--;
	has_next = true;
out:
	if (err != NULL) {
		*err = rv;
	}
	return has_next;
}

uint16_t
sqsh_xattr_iterator_type(const struct SqshXattrIterator *iterator) {
	return sqsh__data_xattr_key_type(get_key(iterator)) & (uint16_t)~0x0100;
}

bool
sqsh_xattr_iterator_is_indirect(const struct SqshXattrIterator *iterator) {
	return (sqsh__data_xattr_key_type(get_key(iterator)) & (uint16_t)0x0100) !=
			0;
}

const char *
sqsh_xattr_iterator_name(const struct SqshXattrIterator *iterator) {
	return (const char *)sqsh__data_xattr_key_name(get_key(iterator));
}

const char *
sqsh_xattr_iterator_prefix(const struct SqshXattrIterator *iterator) {
	switch (sqsh_xattr_iterator_type(iterator)) {
	case SQSH_XATTR_USER:
		return "user.";
	case SQSH_XATTR_TRUSTED:
		return "trusted.";
	case SQSH_XATTR_SECURITY:
		return "security.";
	}
	return NULL;
}

uint16_t
sqsh_xattr_iterator_prefix_size(const struct SqshXattrIterator *iterator) {
	switch (sqsh_xattr_iterator_type(iterator)) {
	case SQSH_XATTR_USER:
		return 5;
	case SQSH_XATTR_TRUSTED:
		return 8;
	case SQSH_XATTR_SECURITY:
		return 9;
	}
	return 0;
}

int
sqsh_xattr_iterator_lookup(
		struct SqshXattrIterator *iterator, const char *name) {
	int rv = 0, rv_cmp = 0;

	while (sqsh_xattr_iterator_next(iterator, &rv)) {
		rv_cmp = sqsh_xattr_iterator_fullname_cmp(iterator, name);
		if (rv_cmp == 0) {
			return 0;
		} else if (rv_cmp > 0) {
			return -SQSH_ERROR_NO_SUCH_XATTR;
		}
	}
	if (rv < 0) {
		return rv;
	} else {
		return -SQSH_ERROR_NO_SUCH_XATTR;
	}
}
int
sqsh_xattr_iterator_fullname_cmp(
		const struct SqshXattrIterator *iterator, const char *name) {
	int rv = 0;
	const char *prefix = sqsh_xattr_iterator_prefix(iterator);
	const size_t prefix_len = sqsh_xattr_iterator_prefix_size(iterator);
	rv = strncmp(name, prefix, prefix_len);
	if (rv != 0) {
		return rv;
	}
	name += prefix_len;

	const char *xattr_name = sqsh_xattr_iterator_name(iterator);
	if (strlen(name) != sqsh_xattr_iterator_name_size(iterator)) {
		return -1;
	} else {
		return strncmp(
				name, xattr_name, sqsh_xattr_iterator_name_size(iterator));
	}
}

char *
sqsh_xattr_iterator_fullname_dup(const struct SqshXattrIterator *iterator) {
	const char *prefix = sqsh_xattr_iterator_prefix(iterator);
	const char *name = sqsh_xattr_iterator_name(iterator);

	const size_t prefix_size = strlen(prefix);
	const size_t name_size = sqsh_xattr_iterator_name_size(iterator);
	const size_t size = prefix_size + name_size;

	char *fullname_buffer = calloc(size + 1, sizeof(char));
	if (fullname_buffer == NULL) {
		return NULL;
	}
	memcpy(fullname_buffer, prefix, prefix_size);
	memcpy(&fullname_buffer[prefix_size], name, name_size);
	return fullname_buffer;
}

uint16_t
sqsh_xattr_iterator_name_size(const struct SqshXattrIterator *iterator) {
	return sqsh__data_xattr_key_name_size(get_key(iterator));
}

char *
sqsh_xattr_iterator_value_dup(const struct SqshXattrIterator *iterator) {
	const size_t size = sqsh_xattr_iterator_value_size2(iterator);
	const char *value = sqsh_xattr_iterator_value(iterator);

	return cx_memdup(value, size);
}

const char *
sqsh_xattr_iterator_value(const struct SqshXattrIterator *iterator) {
	return (const char *)sqsh__data_xattr_value(get_value(iterator));
}

uint32_t
sqsh_xattr_iterator_value_size2(const struct SqshXattrIterator *iterator) {
	return sqsh__data_xattr_value_size(get_value(iterator));
}

uint16_t
sqsh_xattr_iterator_value_size(const struct SqshXattrIterator *iterator) {
	return (uint16_t)sqsh_xattr_iterator_value_size2(iterator);
}

int
sqsh__xattr_iterator_cleanup(struct SqshXattrIterator *iterator) {
	sqsh__metablock_reader_cleanup(&iterator->out_of_line_value);
	sqsh__metablock_reader_cleanup(&iterator->metablock);
	return 0;
}

int
sqsh_xattr_iterator_free(struct SqshXattrIterator *iterator) {
	SQSH_FREE_IMPL(sqsh__xattr_iterator_cleanup, iterator);
}
