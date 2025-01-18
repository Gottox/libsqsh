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
 * @file         sqsh_archive_builder.h
 */

#ifndef MKSQSH_TABLE_H
#define MKSQSH_TABLE_H

#include <mksqsh_metablock.h>
#include <sqsh_data.h>
#include <sqsh_data_set.h>

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************
 * table/table_builder.c
 */

struct MksqshTable {
	struct MksqshMetablock metablock_writer;
	size_t entry_size;
	size_t entry_count;
	FILE *output;
	FILE *metablock_output;
};

int mksqsh__table_init(
		struct MksqshTable *table, size_t entry_size, FILE *content_output,
		FILE *lookup_output);

int mksqsh__table_add(
		struct MksqshTable *table, const void *entry, size_t entry_size);

int mksqsh__table_flush(struct MksqshTable *table);

int mksqsh__table_cleanup(struct MksqshTable *table);

/***************************************
 * table/fragment_table_builder.c
 */

struct MksqshFragmentTable {
	struct MksqshTable table;
};

int mksqsh__fragment_table_init(
		struct MksqshFragmentTable *table, FILE *content_output,
		FILE *lookup_output);

int mksqsh__fragment_table_add(
		struct MksqshFragmentTable *table, uint64_t start, uint32_t size);

int mksqsh__fragment_table_flush(struct MksqshFragmentTable *table);

int mksqsh__fragment_table_cleanup(struct MksqshFragmentTable *table);

/***************************************
 * table/id_table_builder.c
 */

struct MksqshIdTable {
	struct MksqshTable table;
};

int mksqsh__id_table_init(
		struct MksqshIdTable *table, FILE *content_output, FILE *lookup_output);

int mksqsh__id_table_add(struct MksqshIdTable *table, uint32_t id);

int mksqsh__id_table_flush(struct MksqshIdTable *table);

int mksqsh__id_table_cleanup(struct MksqshIdTable *table);

#ifdef __cplusplus
}
#endif
#endif /* MKSQSH_TABLE_H */
