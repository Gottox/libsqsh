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
 * @file        : xattr
 * @created     : Friday May 07, 2021 07:03:51 CEST
 */

#include <stdint.h>

#ifndef SQUASH_XATTR_H

#define SQUASH_XATTR_H

#define SQUASH_SIZEOF_XATTR_KEY 4
#define SQUASH_SIZEOF_XATTR_VALUE 4
#define SQUASH_SIZEOF_XATTR_LOOKUP_TABLE 16
#define SQUASH_SIZEOF_XATTR_ID_TABLE 16

struct SquashXattrKey;

struct SquashXattrValue;

struct SquashXattrLookupTable;

struct SquashXattrIdTable;

uint16_t squash_data_xattr_key_type(const struct SquashXattrKey *xattr_key);
uint16_t
squash_data_xattr_key_name_size(const struct SquashXattrKey *xattr_key);
const uint8_t *
squash_data_xattr_key_name(const struct SquashXattrKey *xattr_key);

uint32_t
squash_data_xattr_value_size(const struct SquashXattrValue *xattr_value);
const uint8_t *
squash_data_xattr_value(const struct SquashXattrValue *xattr_value);

uint64_t squash_data_xattr_lookup_table_xattr_ref(
		const struct SquashXattrLookupTable *lookup_table);
uint32_t squash_data_xattr_lookup_table_count(
		const struct SquashXattrLookupTable *lookup_table);
uint32_t squash_data_xattr_lookup_table_size(
		const struct SquashXattrLookupTable *lookup_table);

uint64_t squash_data_xattr_id_table_xattr_table_start(
		const struct SquashXattrIdTable *xattr_id_table);
uint32_t squash_data_xattr_id_table_xattr_ids(
		const struct SquashXattrIdTable *xattr_id_table);
const uint64_t *
squash_data_xattr_id_table(const struct SquashXattrIdTable *xattr_id_table);

#endif /* end of include guard SQUASH_XATTR_H */
