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
 * @author   Enno Boland (mail@eboland.de)
 * @file     util.c
 * @created  Saturday Feb 18, 2023 14:06:14 CET
 */

// for fmemopen
#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <mksqsh_archive.h>
#include <mksqsh_table.h>
#include <sqsh_archive_private.h>
#include <sqsh_data_private.h>
#include <sqsh_mapper.h>

FILE *
test_sqsh_prepare_archive(uint8_t *payload, size_t payload_size) {
	int rv;
	const int compression_id =
			payload[20] ? payload[20] : SQSH_COMPRESSION_GZIP;
	struct MksqshSuperblock builder = {0};
	struct MksqshIdTable id_builder = {0};
	FILE *fsuperblock = fmemopen(payload, payload_size, "r+");
	assert(fsuperblock);
	rv = mksqsh__superblock_init(&builder);
	assert(rv == 0);
	rv = mksqsh__superblock_fragment_count(&builder, 0);
	assert(rv == 0);
	rv = mksqsh__superblock_compress_inodes(&builder, true);
	assert(rv == 0);
	rv = mksqsh__superblock_compress_data(&builder, true);
	assert(rv == 0);
	rv = mksqsh__superblock_compress_fragments(&builder, true);
	assert(rv == 0);
	rv = mksqsh__superblock_force_fragments(&builder, true);
	assert(rv == 0);
	rv = mksqsh__superblock_deduplicate(&builder, true);
	assert(rv == 0);
	rv = mksqsh__superblock_compress_xattr(&builder, true);
	assert(rv == 0);
	rv = mksqsh__superblock_compression_options(&builder, true);
	assert(rv == 0);
	rv = mksqsh__superblock_id_count(&builder, 2);
	assert(rv == 0);
	rv = mksqsh__superblock_inode_count(&builder, 100);
	assert(rv == 0);
	rv = mksqsh__superblock_root_inode_ref(&builder, 0);
	assert(rv == 0);
	rv = mksqsh__superblock_modification_time(&builder, 0);
	assert(rv == 0);
	rv = mksqsh__superblock_block_size(&builder, 32768);
	assert(rv == 0);
	rv = mksqsh__superblock_compression_id(&builder, compression_id);
	assert(rv == 0);
	rv = mksqsh__superblock_bytes_used(&builder, payload_size);
	assert(rv == 0);
	rv = mksqsh__superblock_id_table_start(&builder, ID_TABLE_OFFSET);
	assert(rv == 0);
	rv = mksqsh__superblock_xattr_table_start(&builder, XATTR_TABLE_OFFSET);
	assert(rv == 0);
	rv = mksqsh__superblock_inode_table_start(&builder, INODE_TABLE_OFFSET);
	assert(rv == 0);
	rv = mksqsh__superblock_directory_table_start(
			&builder, DIRECTORY_TABLE_OFFSET);
	assert(rv == 0);
	rv = mksqsh__superblock_fragment_table_start(
			&builder, FRAGMENT_TABLE_OFFSET);
	assert(rv == 0);
	rv = mksqsh__superblock_export_table_start(&builder, EXPORT_TABLE_OFFSET);
	assert(rv == 0);
	rv = mksqsh__superblock_write(&builder, fsuperblock);
	assert(rv == sizeof(struct SqshDataSuperblock));
	rv = mksqsh__superblock_cleanup(&builder);
	assert(rv == 0);

	FILE *fid_block = fmemopen(
			&payload[ID_TABLE_OFFSET], payload_size - ID_TABLE_OFFSET, "r+");
	char *id_block_tmp;
	size_t id_block_tmp_size;
	FILE *fid_block_tmp = open_memstream(&id_block_tmp, &id_block_tmp_size);
	assert(fid_block);
	assert(fid_block_tmp);

	rv = mksqsh__id_table_init(&id_builder, fid_block, fid_block_tmp);
	assert(rv == 0);
	rv = mksqsh__id_table_add(&id_builder, 123);
	assert(rv == 0);
	rv = mksqsh__id_table_add(&id_builder, 456);
	assert(rv == 0);
	rv = mksqsh__id_table_flush(&id_builder);
	assert(rv == 0);
	rv = mksqsh__id_table_cleanup(&id_builder);
	assert(rv == 0);

	fclose(fid_block);
	fclose(fid_block_tmp);
	free(id_block_tmp);

	return fsuperblock;
}

void
test_sqsh_init_archive(
		struct SqshArchive *sqsh, FILE *farchive, uint8_t *payload,
		size_t payload_size) {
	int rv;
	fseek(farchive, payload_size, SEEK_SET);
	fclose(farchive);

	const struct SqshConfig config = DEFAULT_CONFIG(payload_size);
	rv = sqsh__archive_init(sqsh, payload, &config);
	assert(0 == rv);
}

const uint8_t *
mk_stub(struct SqshArchive *sqsh, uint8_t *payload, size_t payload_size) {
	FILE *farchive = test_sqsh_prepare_archive(payload, payload_size);
	test_sqsh_init_archive(sqsh, farchive, payload, payload_size);
	return payload;
}
