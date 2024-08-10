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
 * @file         file.c
 */

#define _DEFAULT_SOURCE

#include <sqsh_easy.h>

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <sqsh_error.h>
#include <sqsh_file_private.h>
#include <sqsh_tree_private.h>

bool
sqsh_easy_file_exists(struct SqshArchive *archive, const char *path, int *err) {
	int rv = 0;
	struct SqshPathResolver resolver = {0};
	bool exists = false;

	rv = sqsh__path_resolver_init(&resolver, archive);
	if (rv < 0) {
		goto out;
	}
	rv = sqsh_path_resolver_resolve(&resolver, path, true);
	if (rv == -SQSH_ERROR_NO_SUCH_FILE) {
		rv = 0;
		goto out;
	} else if (rv < 0) {
		goto out;
	}

	exists = true;

out:
	sqsh__path_resolver_cleanup(&resolver);
	if (err != NULL) {
		*err = rv;
	}
	return exists;
}

uint8_t *
sqsh_easy_file_content(
		struct SqshArchive *archive, const char *path, int *err) {
	int rv = 0;
	struct CxBuffer buffer = {0};
	struct SqshFileIterator iterator = {0};
	struct SqshFile *file = NULL;
	uint8_t *content = NULL;

	file = sqsh_open(archive, path, &rv);
	if (rv < 0) {
		goto out;
	}

	rv = sqsh__file_iterator_init(&iterator, file);
	if (rv < 0) {
		goto out;
	}

	const size_t file_size = sqsh_file_size(file);
	rv = cx_buffer_init(&buffer);
	if (rv < 0) {
		goto out;
	}

	rv = cx_buffer_add_capacity_exact(&buffer, NULL, file_size + 1);
	if (rv < 0) {
		goto out;
	}

	while (rv >= 0 && sqsh_file_iterator_next(&iterator, SIZE_MAX, &rv)) {
		const uint8_t *data = sqsh_file_iterator_data(&iterator);
		const size_t size = sqsh_file_iterator_size(&iterator);
		rv = cx_buffer_append(&buffer, data, size);
	}

	if (rv < 0) {
		goto out;
	}
	rv = cx_buffer_append(&buffer, (const uint8_t *)"", 1);
	if (rv < 0) {
		goto out;
	}

	content = cx_buffer_unwrap(&buffer);
out:
	sqsh__file_iterator_cleanup(&iterator);
	sqsh_close(file);
	cx_buffer_cleanup(&buffer);
	if (err != NULL) {
		*err = rv;
	}
	return content;
}

size_t
sqsh_easy_file_size(struct SqshArchive *archive, const char *path, int *err) {
	int rv = 0;
	struct SqshFile *file = NULL;
	size_t file_size = 0;

	file = sqsh_open(archive, path, &rv);
	if (rv < 0) {
		goto out;
	}

	file_size = sqsh_file_size(file);

out:
	sqsh_close(file);
	if (err != NULL) {
		*err = rv;
	}
	return file_size;
}

mode_t
sqsh_easy_file_permission(
		struct SqshArchive *archive, const char *path, int *err) {
	int rv = 0;
	struct SqshFile *file = NULL;
	mode_t permission = 0;

	file = sqsh_open(archive, path, &rv);
	if (rv < 0) {
		goto out;
	}

	permission = sqsh_file_permission(file);

out:
	sqsh_close(file);
	if (err != NULL) {
		*err = rv;
	}
	return permission;
}

time_t
sqsh_easy_file_mtime(struct SqshArchive *archive, const char *path, int *err) {
	int rv = 0;
	struct SqshFile *file = NULL;
	time_t modified = 0;

	file = sqsh_open(archive, path, &rv);
	if (rv < 0) {
		goto out;
	}

	modified = sqsh_file_modified_time(file);

out:
	sqsh_close(file);
	if (err != NULL) {
		*err = rv;
	}
	return modified;
}
