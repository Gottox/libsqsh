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
 * @file         curl_mapper.c
 */

#define _DEFAULT_SOURCE
#include <stddef.h>

#ifdef CONFIG_CURL

#	include <sqsh_mapper_private.h>

#	include <sqsh_data_private.h>
#	include <sqsh_error.h>
#	include <sqsh_common_private.h>

#	include <curl/curl.h>
#	include <inttypes.h>
#	include <string.h>

#	define CONTENT_RANGE "Content-Range"
#	define CONTENT_RANGE_FORMAT "bytes %" PRIu64 "-%" PRIu64 "/%" PRIu64

struct SqshCurlMapper {
	char *url;
	uint64_t expected_time;
	void *handle;
	uint8_t *header_cache;
	sqsh__mutex_t lock;
};

struct SqshCurlWriteInfo {
	uint8_t *buffer;
	size_t offset;
	size_t size;
	int rv;
};

static size_t
write_data(void *ptr, size_t size, size_t nmemb, void *userdata) {
	size_t byte_size;
	size_t next_offset;
	struct SqshCurlWriteInfo *info = userdata;

	if (SQSH_MULT_OVERFLOW(size, nmemb, &byte_size)) {
		info->rv = -SQSH_ERROR_INTEGER_OVERFLOW;
		return 0;
	}
	if (SQSH_ADD_OVERFLOW(info->offset, byte_size, &next_offset)) {
		info->rv = -SQSH_ERROR_INTEGER_OVERFLOW;
		return 0;
	}

	if (next_offset > info->size) {
		info->rv = -SQSH_ERROR_MAPPER_MAP;
		return 0;
	}

	memcpy(&info->buffer[info->offset], ptr, byte_size);

	info->offset = next_offset;

	return byte_size;
}

static int
get_total_size(CURL *handle, uint64_t *total) {
	CURLHcode hcode;
	uint64_t dummy;
	struct curl_header *header = NULL;
	int scanned_fields;

	hcode = curl_easy_header(
			handle, CONTENT_RANGE, 0, CURLH_HEADER, -1, &header);
	if (hcode != CURLHE_OK) {
		return -SQSH_ERROR_CURL_INVALID_RANGE_HEADER;
	}

	scanned_fields =
			sscanf(header->value, CONTENT_RANGE_FORMAT, &dummy, &dummy, total);

	if (scanned_fields != 3) {
		return -SQSH_ERROR_CURL_INVALID_RANGE_HEADER;
	}

	return 0;
}

static int
get_file_time(CURL *handle, uint64_t *file_time) {
	curl_off_t file_time_t;
	CURLcode code;

	code = curl_easy_getinfo(handle, CURLINFO_FILETIME_T, &file_time_t);
	/* According to curl docs, file_time_t is set to -1 if the server does not
	 * report a file time. We treat this as an error as we need to detect if
	 * the file has changed.
	 */
	if (code != CURLE_OK || file_time_t < 0) {
		return -SQSH_ERROR_MAPPER_MAP;
	}
	/* We checked for negative values above, so this cast should be safe. */
	*file_time = (uint64_t)file_time_t;

	return 0;
}

static CURL *
configure_handle(struct SqshCurlMapper *mapper) {
	CURL *handle = mapper->handle;
	const long tls_versions =
			CURL_SSLVERSION_TLSv1_2 | CURL_SSLVERSION_MAX_DEFAULT;
	curl_easy_reset(handle);
	curl_easy_setopt(handle, CURLOPT_URL, mapper->url);
	curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(handle, CURLOPT_FILETIME, 1L);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(handle, CURLOPT_SSLVERSION, tls_versions);
	return handle;
}

static int
curl_download(
		CURL *handle, sqsh_index_t offset, size_t size, uint8_t **data,
		uint64_t *file_size, uint64_t *file_time) {
	*data = calloc(size, sizeof(uint8_t));
	if (*data == NULL) {
		return -SQSH_ERROR_MALLOC_FAILED;
	}

	struct SqshCurlWriteInfo write_info = {
			.size = size,
			.buffer = *data,
	};

	CURLcode code;
	int rv = 0;
	/* The actual max-size this string should ever use is 42, but we
	 * add some padding to be a nice number. Not that 42 isn't nice.
	 */
	char range_buffer[64] = {0};
	const uint64_t end_offset = offset + size - 1;
	long http_code = 0;

	rv = snprintf(
			range_buffer, sizeof(range_buffer), "%zu-%" PRIu64, offset,
			end_offset);
	if (rv >= (int)sizeof(range_buffer)) {
		rv = -SQSH_ERROR_MAPPER_MAP;
		goto out;
	}
	curl_easy_setopt(handle, CURLOPT_RANGE, range_buffer);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, &write_info);

	code = curl_easy_perform(handle);
	if (code != CURLE_OK) {
		rv = -SQSH_ERROR_MAPPER_MAP;
		goto out;
	}

	code = curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &http_code);
	if (http_code != 206 || code != CURLE_OK) {
		rv = -SQSH_ERROR_MAPPER_MAP;
		goto out;
	}

	rv = get_total_size(handle, file_size);
	if (rv < 0) {
		goto out;
	}

	rv = get_file_time(handle, file_time);
	if (rv < 0) {
		goto out;
	}

out:
	return rv;
}

static int
sqsh_mapper_curl_init(
		struct SqshMapper *mapper, const void *input, size_t *size) {
	(void)size;
	int rv = 0;
	curl_global_init(CURL_GLOBAL_ALL);

	struct SqshCurlMapper *curl_mapper =
			calloc(1, sizeof(struct SqshCurlMapper));
	if (curl_mapper == NULL) {
		rv = -SQSH_ERROR_MALLOC_FAILED;
		goto out;
	}
	curl_mapper->url = strdup(input);
	curl_mapper->handle = curl_easy_init();
	sqsh_mapper_set_user_data(mapper, curl_mapper);

	rv = sqsh__mutex_init(&curl_mapper->lock);
	if (rv < 0) {
		goto out;
	}

	size_t block_size = sqsh_mapper_block_size(mapper);
	CURL *handle = configure_handle(curl_mapper);

	uint64_t size64 = *size;
	rv = curl_download(
			handle, 0, block_size, &curl_mapper->header_cache, &size64,
			&curl_mapper->expected_time);
	if (rv < 0) {
		goto out;
	}
	if (size64 > SIZE_MAX) {
		rv = -SQSH_ERROR_MAPPER_INIT;
		goto out;
	}
	*size = (size_t)size64;

out:
	return rv;
}

static int
sqsh_mapper_curl_map(
		const struct SqshMapper *mapper, sqsh_index_t offset, size_t size,
		uint8_t **data) {
	struct SqshCurlMapper *curl_mapper = sqsh_mapper_user_data(mapper);
	int rv = 0;
	uint64_t file_size = 0;
	uint64_t file_time = 0;

	sqsh__mutex_t *lock = &curl_mapper->lock;
	rv = sqsh__mutex_lock(lock);
	if (rv < 0) {
		goto out;
	}
	if (offset == 0 && curl_mapper->header_cache != NULL) {
		*data = curl_mapper->header_cache;
		curl_mapper->header_cache = NULL;
	} else {
		CURL *handle = configure_handle(mapper->user_data);

		rv = curl_download(handle, offset, size, data, &file_size, &file_time);
		if (rv < 0) {
			goto out;
		}

		if (file_time != curl_mapper->expected_time) {
			rv = -SQSH_ERROR_MAPPER_MAP;
			goto out;
		}

		if (file_size != sqsh_mapper_size(mapper)) {
			rv = -SQSH_ERROR_MAPPER_MAP;
			goto out;
		}
	}

out:
	sqsh__mutex_unlock(lock);
	return rv;
}

static int
sqsh_mapper_curl_cleanup(struct SqshMapper *mapper) {
	struct SqshCurlMapper *user_data = mapper->user_data;

	free(user_data->url);
	free(user_data->header_cache);
	sqsh__mutex_destroy(&user_data->lock);
	curl_easy_cleanup(user_data->handle);
	return 0;
}

static int
sqsh_mapping_curl_unmap(
		const struct SqshMapper *mapper, uint8_t *data, size_t size) {
	(void)mapper;
	(void)size;
	free(data);
	return 0;
}

static const struct SqshMemoryMapperImpl impl = {
		/* 40kb */
		.block_size_hint = 40 * 1024,     .init = sqsh_mapper_curl_init,
		.map = sqsh_mapper_curl_map,      .cleanup = sqsh_mapper_curl_cleanup,
		.unmap = sqsh_mapping_curl_unmap,
};
const struct SqshMemoryMapperImpl *const sqsh_mapper_impl_curl = &impl;
#else
const struct SqshMemoryMapperImpl *const sqsh_mapper_impl_curl = NULL;
#endif
