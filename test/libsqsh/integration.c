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
 * @file         integration.c
 */

#include "common.h"
#include <pthread.h>
#include <sqsh_archive_private.h>
#include <sqsh_content_private.h>
#include <sqsh_directory_private.h>
#include <sqsh_easy.h>
#include <sqsh_file_private.h>
#include <sqsh_tree.h>
#include <sqsh_tree_private.h>
#include <utest.h>

extern uint8_t squashfs_image[];
extern size_t squashfs_image_size;

#define TEST_SQUASHFS_IMAGE_LEN squashfs_image_size
#define TEST_SQUASHFS_IMAGE squashfs_image

UTEST(integration, sqsh_empty) {
	int rv;
	struct SqshArchive sqsh = {0};
	struct SqshConfig config = DEFAULT_CONFIG(0);
	rv = sqsh__archive_init(&sqsh, NULL, &config);
	ASSERT_EQ(-SQSH_ERROR_SUPERBLOCK_TOO_SMALL, rv);
}

UTEST(integration, sqsh_get_nonexistant) {
	int rv;
	struct SqshArchive sqsh = {0};
	struct SqshPathResolver resolver = {0};

	struct SqshConfig config = DEFAULT_CONFIG(TEST_SQUASHFS_IMAGE_LEN);
	config.archive_offset = 1010;
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	ASSERT_EQ(0, rv);

	rv = sqsh__path_resolver_init(&resolver, &sqsh);
	ASSERT_EQ(0, rv);

	rv = sqsh_path_resolver_to_root(&resolver);
	ASSERT_EQ(0, rv);

	rv = sqsh_path_resolver_resolve(&resolver, "/nonexistant", false);
	ASSERT_GT(0, rv);

	rv = sqsh__path_resolver_cleanup(&resolver);
	ASSERT_EQ(0, rv);

	rv = sqsh__archive_cleanup(&sqsh);
	ASSERT_EQ(0, rv);
}

UTEST(integration, path_resolver) {
	int rv;
	struct SqshPathResolver resolver = {0};
	struct SqshArchive sqsh = {0};
	struct SqshFile *file;
	struct SqshConfig config = DEFAULT_CONFIG(TEST_SQUASHFS_IMAGE_LEN);
	config.archive_offset = 1010;
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	ASSERT_EQ(0, rv);

	rv = sqsh__path_resolver_init(&resolver, &sqsh);
	ASSERT_EQ(0, rv);

	rv = sqsh_path_resolver_to_root(&resolver);
	ASSERT_EQ(0, rv);

	rv = sqsh_path_resolver_resolve(&resolver, "/large_dir", false);
	ASSERT_EQ(0, rv);

	rv = sqsh_path_resolver_resolve(&resolver, "999", false);
	ASSERT_EQ(0, rv);

	file = sqsh_path_resolver_open_file(&resolver, &rv);
	ASSERT_NE(NULL, file);
	ASSERT_EQ(0, rv);

	rv = sqsh_close(file);
	ASSERT_EQ(0, rv);

	rv = sqsh__path_resolver_cleanup(&resolver);
	ASSERT_EQ(0, rv);

	rv = sqsh__archive_cleanup(&sqsh);
	ASSERT_EQ(0, rv);
}

UTEST(integration, sqsh_ls) {
	int rv;
	char *name;
	struct SqshFile *file = NULL;
	struct SqshDirectoryIterator *iter = NULL;
	struct SqshArchive sqsh = {0};
	struct SqshConfig config = DEFAULT_CONFIG(TEST_SQUASHFS_IMAGE_LEN);
	config.archive_offset = 1010;
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	ASSERT_EQ(0, rv);

	file = sqsh_open(&sqsh, "/", &rv);
	ASSERT_EQ(0, rv);

	iter = sqsh_directory_iterator_new(file, &rv);
	ASSERT_NE(NULL, iter);
	ASSERT_EQ(0, rv);

	bool has_next = sqsh_directory_iterator_next(iter, &rv);
	ASSERT_EQ(0, rv);
	assert(has_next);
	name = sqsh_directory_iterator_name_dup(iter);
	ASSERT_NE(NULL, name);
	ASSERT_EQ(0, strcmp("a", name));
	free(name);

	has_next = sqsh_directory_iterator_next(iter, &rv);
	assert(rv >= 0);
	ASSERT_EQ(true, has_next);
	name = sqsh_directory_iterator_name_dup(iter);
	ASSERT_NE(NULL, name);
	ASSERT_EQ(0, strcmp("b", name));
	free(name);

	has_next = sqsh_directory_iterator_next(iter, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(true, has_next);
	name = sqsh_directory_iterator_name_dup(iter);
	ASSERT_NE(NULL, name);
	ASSERT_EQ(0, strcmp("large_dir", name));
	free(name);

	has_next = sqsh_directory_iterator_next(iter, &rv);
	// End of file list
	ASSERT_EQ(0, rv);
	ASSERT_EQ(false, has_next);

	rv = sqsh_directory_iterator_free(iter);
	ASSERT_EQ(0, rv);

	rv = sqsh_close(file);
	ASSERT_EQ(0, rv);

	rv = sqsh__archive_cleanup(&sqsh);
	ASSERT_EQ(0, rv);
}

UTEST(integration, sqsh_read_content) {
	int rv;
	char *data;
	struct SqshArchive archive = {0};
	struct SqshConfig config = DEFAULT_CONFIG(TEST_SQUASHFS_IMAGE_LEN);
	config.archive_offset = 1010;
	rv = sqsh__archive_init(&archive, (char *)TEST_SQUASHFS_IMAGE, &config);
	ASSERT_EQ(0, rv);

	data = (char *)sqsh_easy_file_content(&archive, "/a", NULL);
	ASSERT_EQ(0, strcmp(data, "a\n"));
	free(data);

	rv = sqsh__archive_cleanup(&archive);
	ASSERT_EQ(0, rv);
}

UTEST(integration, sqsh_cat_fragment) {
	int rv;
	const uint8_t *data;
	size_t size;
	struct SqshFile *file = NULL;
	struct SqshFileReader reader = {0};
	struct SqshArchive sqsh = {0};
	struct SqshPathResolver resolver = {0};
	struct SqshConfig config = DEFAULT_CONFIG(TEST_SQUASHFS_IMAGE_LEN);
	config.archive_offset = 1010;
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	ASSERT_EQ(0, rv);

	rv = sqsh__path_resolver_init(&resolver, &sqsh);
	ASSERT_EQ(0, rv);

	rv = sqsh_path_resolver_to_root(&resolver);
	ASSERT_EQ(0, rv);

	rv = sqsh_path_resolver_resolve(&resolver, "a", false);
	ASSERT_EQ(0, rv);

	file = sqsh_path_resolver_open_file(&resolver, &rv);
	ASSERT_NE(NULL, file);
	ASSERT_EQ(0, rv);

	rv = sqsh__file_reader_init(&reader, file);
	ASSERT_EQ(0, rv);

	size = sqsh_file_size(file);
	ASSERT_EQ((size_t)2, size);

	rv = sqsh_file_reader_advance2(&reader, 0, size);
	ASSERT_EQ(0, rv);

	data = sqsh_file_reader_data(&reader);
	ASSERT_EQ(0, memcmp(data, "a\n", size));

	rv = sqsh__file_reader_cleanup(&reader);
	ASSERT_EQ(0, rv);

	rv = sqsh_close(file);
	ASSERT_EQ(0, rv);

	rv = sqsh__path_resolver_cleanup(&resolver);
	ASSERT_EQ(0, rv);

	rv = sqsh__archive_cleanup(&sqsh);
	ASSERT_EQ(0, rv);
}

UTEST(integration, sqsh_cat_datablock_and_fragment) {
	int rv;
	const uint8_t *data;
	size_t size;
	struct SqshFile *file = NULL;
	struct SqshFileReader reader = {0};
	struct SqshArchive sqsh = {0};
	struct SqshPathResolver resolver = {0};
	const struct SqshConfig config = {
			.source_mapper = sqsh_mapper_impl_static,
			.source_size = TEST_SQUASHFS_IMAGE_LEN,
			.archive_offset = 1010,
	};
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	ASSERT_EQ(0, rv);

	rv = sqsh__path_resolver_init(&resolver, &sqsh);
	ASSERT_EQ(0, rv);

	rv = sqsh_path_resolver_to_root(&resolver);
	ASSERT_EQ(0, rv);

	rv = sqsh_path_resolver_resolve(&resolver, "b", false);
	ASSERT_EQ(0, rv);

	file = sqsh_path_resolver_open_file(&resolver, &rv);
	ASSERT_NE(NULL, file);
	ASSERT_EQ(0, rv);

	rv = sqsh__file_reader_init(&reader, file);
	ASSERT_EQ(0, rv);

	size = sqsh_file_size(file);
	ASSERT_EQ((size_t)1050000, size);

	rv = sqsh_file_reader_advance(&reader, 0, size);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(sqsh_file_reader_size(&reader), size);

	data = sqsh_file_reader_data(&reader);
	for (sqsh_index_t i = 0; i < size; i++) {
		ASSERT_EQ('b', data[i]);
	}

	rv = sqsh__file_reader_cleanup(&reader);
	ASSERT_EQ(0, rv);

	rv = sqsh_close(file);
	ASSERT_EQ(0, rv);

	rv = sqsh__path_resolver_cleanup(&resolver);
	ASSERT_EQ(0, rv);

	rv = sqsh__archive_cleanup(&sqsh);
	ASSERT_EQ(0, rv);
}

UTEST(integration, sqsh_cat_size_overflow) {
	int rv;
	size_t size;
	struct SqshFile *file = NULL;
	struct SqshFileReader reader = {0};
	struct SqshArchive sqsh = {0};
	struct SqshPathResolver resolver = {0};
	const struct SqshConfig config = {
			.source_mapper = sqsh_mapper_impl_static,
			.source_size = TEST_SQUASHFS_IMAGE_LEN,
			.archive_offset = 1010,
	};
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	ASSERT_EQ(0, rv);

	rv = sqsh__path_resolver_init(&resolver, &sqsh);
	ASSERT_EQ(0, rv);

	rv = sqsh_path_resolver_to_root(&resolver);
	ASSERT_EQ(0, rv);

	rv = sqsh_path_resolver_resolve(&resolver, "b", false);
	ASSERT_EQ(0, rv);

	file = sqsh_path_resolver_open_file(&resolver, &rv);
	ASSERT_NE(NULL, file);
	ASSERT_EQ(0, rv);

	rv = sqsh__file_reader_init(&reader, file);
	ASSERT_EQ(0, rv);
	size = sqsh_file_size(file);
	ASSERT_EQ((size_t)1050000, size);

	rv = sqsh_file_reader_advance(&reader, 0, size + 4096);
	ASSERT_EQ(-SQSH_ERROR_OUT_OF_BOUNDS, rv);

	rv = sqsh__file_reader_cleanup(&reader);
	ASSERT_EQ(0, rv);

	rv = sqsh_close(file);
	ASSERT_EQ(0, rv);

	rv = sqsh__path_resolver_cleanup(&resolver);
	ASSERT_EQ(0, rv);

	rv = sqsh__archive_cleanup(&sqsh);
	ASSERT_EQ(0, rv);
}

UTEST(integration, sqsh_test_uid_and_gid) {
	int rv;
	uint32_t uid, gid;
	struct SqshFile *file = NULL;
	struct SqshArchive sqsh = {0};
	const struct SqshConfig config = {
			.source_mapper = sqsh_mapper_impl_static,
			.source_size = TEST_SQUASHFS_IMAGE_LEN,
			.archive_offset = 1010,
	};
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	ASSERT_EQ(0, rv);

	file = sqsh_open(&sqsh, "/", &rv);
	ASSERT_EQ(0, rv);

	uid = sqsh_file_uid(file);
	ASSERT_EQ((uint32_t)2020, uid);
	gid = sqsh_file_gid(file);
	ASSERT_EQ((uint32_t)202020, gid);

	rv = sqsh_close(file);
	ASSERT_EQ(0, rv);

	rv = sqsh__archive_cleanup(&sqsh);
	ASSERT_EQ(0, rv);
}

UTEST(integration, sqsh_test_extended_dir) {
	int rv;
	struct SqshFile *file = NULL;
	struct SqshArchive sqsh = {0};
	struct SqshPathResolver resolver = {0};
	const struct SqshConfig config = {
			.source_mapper = sqsh_mapper_impl_static,
			.source_size = TEST_SQUASHFS_IMAGE_LEN,
			.archive_offset = 1010,
	};
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	ASSERT_EQ(0, rv);

	rv = sqsh__path_resolver_init(&resolver, &sqsh);
	ASSERT_EQ(0, rv);

	rv = sqsh_path_resolver_to_root(&resolver);
	ASSERT_EQ(0, rv);

	rv = sqsh_path_resolver_resolve(&resolver, "/large_dir/999", false);
	ASSERT_EQ(0, rv);

	file = sqsh_path_resolver_open_file(&resolver, &rv);
	ASSERT_NE(NULL, file);

	rv = sqsh_close(file);
	ASSERT_EQ(0, rv);

	rv = sqsh__path_resolver_cleanup(&resolver);

	rv = sqsh__archive_cleanup(&sqsh);
	ASSERT_EQ(0, rv);
}

#if !defined(__OpenBSD__) && !defined(__FreeBSD__)
UTEST(integration, sqsh_test_xattr) {
	const char *expected_value = "1234567891234567891234567890001234567890";
	int rv;
	char *name, *value;
	struct SqshFile *file = NULL;
	struct SqshFile *entry_file = NULL;
	struct SqshDirectoryIterator *dir_iter = NULL;
	struct SqshXattrIterator *xattr_iter = NULL;
	struct SqshArchive sqsh = {0};
	struct SqshConfig config = {
			.source_mapper = sqsh_mapper_impl_static,
			.source_size = TEST_SQUASHFS_IMAGE_LEN,
			.archive_offset = 1010,
	};
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	ASSERT_EQ(0, rv);

	file = sqsh_open(&sqsh, "/", &rv);
	ASSERT_EQ(0, rv);

	bool has_next;
	xattr_iter = sqsh_xattr_iterator_new(file, &rv);
	ASSERT_NE(NULL, xattr_iter);
	ASSERT_EQ(0, rv);
	has_next = sqsh_xattr_iterator_next(xattr_iter, &rv);
	ASSERT_FALSE(has_next);
	ASSERT_EQ(0, rv);
	rv = sqsh_xattr_iterator_free(xattr_iter);
	ASSERT_EQ(0, rv);

	dir_iter = sqsh_directory_iterator_new(file, &rv);
	ASSERT_NE(NULL, dir_iter);
	ASSERT_EQ(0, rv);

	has_next = sqsh_directory_iterator_next(dir_iter, &rv);
	ASSERT_TRUE(has_next);
	ASSERT_EQ(0, rv);
	name = sqsh_directory_iterator_name_dup(dir_iter);
	ASSERT_NE(NULL, name);
	ASSERT_EQ(0, strcmp("a", name));
	free(name);
	entry_file = sqsh_directory_iterator_open_file(dir_iter, &rv);
	ASSERT_EQ(0, rv);
	xattr_iter = sqsh_xattr_iterator_new(entry_file, &rv);
	ASSERT_NE(NULL, xattr_iter);
	ASSERT_EQ(0, rv);
	has_next = sqsh_xattr_iterator_next(xattr_iter, &rv);
	ASSERT_TRUE(has_next);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(false, sqsh_xattr_iterator_is_indirect(xattr_iter));
	name = sqsh_xattr_iterator_fullname_dup(xattr_iter);
	ASSERT_NE(NULL, name);
	ASSERT_EQ(0, strcmp("user.foo", name));
	free(name);
	value = sqsh_xattr_iterator_value_dup(xattr_iter);
	ASSERT_NE(NULL, value);
	ASSERT_EQ(0, strcmp(expected_value, value));
	free(value);
	has_next = sqsh_xattr_iterator_next(xattr_iter, &rv);
	ASSERT_FALSE(has_next);
	ASSERT_EQ(0, rv);
	rv = sqsh_xattr_iterator_free(xattr_iter);
	ASSERT_EQ(0, rv);
	rv = sqsh_close(entry_file);
	ASSERT_EQ(0, rv);

	has_next = sqsh_directory_iterator_next(dir_iter, &rv);
	ASSERT_TRUE(has_next);
	ASSERT_EQ(0, rv);
	name = sqsh_directory_iterator_name_dup(dir_iter);
	ASSERT_NE(NULL, name);
	ASSERT_EQ(0, strcmp("b", name));
	free(name);
	entry_file = sqsh_directory_iterator_open_file(dir_iter, &rv);
	ASSERT_EQ(0, rv);
	xattr_iter = sqsh_xattr_iterator_new(entry_file, &rv);
	ASSERT_NE(NULL, xattr_iter);
	ASSERT_EQ(0, rv);
	has_next = sqsh_xattr_iterator_next(xattr_iter, &rv);
	ASSERT_TRUE(has_next);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(true, sqsh_xattr_iterator_is_indirect(xattr_iter));
	name = sqsh_xattr_iterator_fullname_dup(xattr_iter);
	ASSERT_NE(NULL, name);
	ASSERT_EQ(0, strcmp("user.bar", name));
	free(name);
	value = sqsh_xattr_iterator_value_dup(xattr_iter);
	ASSERT_NE(NULL, value);
	ASSERT_EQ(0, strcmp(expected_value, value));
	free(value);
	has_next = sqsh_xattr_iterator_next(xattr_iter, &rv);
	ASSERT_FALSE(has_next);
	ASSERT_EQ(0, rv);
	rv = sqsh_xattr_iterator_free(xattr_iter);
	ASSERT_EQ(0, rv);
	rv = sqsh_close(entry_file);
	ASSERT_EQ(0, rv);

	rv = sqsh_directory_iterator_free(dir_iter);
	ASSERT_EQ(0, rv);

	rv = sqsh_close(file);
	ASSERT_EQ(0, rv);

	rv = sqsh__archive_cleanup(&sqsh);
	ASSERT_EQ(0, rv);
}
#endif

struct Walker {
	struct SqshArchive *sqsh;
	uint64_t inode_number;
};

static void *
multithreaded_resolver(void *arg) {
	int rv;
	struct Walker *resolver = arg;
	struct Walker my_resolver = {
			.sqsh = resolver->sqsh,
	};

	struct SqshFile *file =
			sqsh_open_by_ref(resolver->sqsh, resolver->inode_number, &rv);

	if (sqsh_file_type(file) == SQSH_FILE_TYPE_DIRECTORY) {
		struct SqshDirectoryIterator *iter =
				sqsh_directory_iterator_new(file, &rv);
		while (sqsh_directory_iterator_next(iter, &rv) > 0) {
			multithreaded_resolver(&my_resolver);
		}
		sqsh_directory_iterator_free(iter);
	} else {
		struct SqshFileReader *reader = sqsh_file_reader_new(file, &rv);
		size_t size = sqsh_file_size(file);
		rv = sqsh_file_reader_advance2(reader, 0, size);
		assert(rv == 0);
		sqsh_file_reader_free(reader);
	}

	sqsh_close(file);

	return 0;
}

UTEST(integration, multithreaded) {
	int rv;
	pthread_t threads[16] = {0};
	struct SqshArchive sqsh = {0};

	struct SqshConfig config = DEFAULT_CONFIG(TEST_SQUASHFS_IMAGE_LEN);
	config.archive_offset = 1010;
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	ASSERT_EQ(0, rv);

	const struct SqshSuperblock *superblock = sqsh_archive_superblock(&sqsh);
	struct Walker resolver = {
			.sqsh = &sqsh,
			.inode_number = sqsh_superblock_inode_root_ref(superblock),
	};
	for (unsigned long i = 0; i < LENGTH(threads); i++) {
		rv = pthread_create(
				&threads[i], NULL, multithreaded_resolver, &resolver);
		ASSERT_EQ(0, rv);
	}

	for (unsigned long i = 0; i < LENGTH(threads); i++) {
		rv = pthread_join(threads[i], NULL);
		ASSERT_EQ(0, rv);
	}

	rv = sqsh__archive_cleanup(&sqsh);
	ASSERT_EQ(0, rv);
}

UTEST(integration, test_follow_symlink) {
	int rv;
	struct SqshArchive sqsh = {0};

	struct SqshConfig config = DEFAULT_CONFIG(TEST_SQUASHFS_IMAGE_LEN);
	config.archive_offset = 1010;
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	ASSERT_EQ(0, rv);

	char **dir_list = sqsh_easy_directory_list(
			&sqsh, "/large_dir/link/large_dir/link", &rv);
	ASSERT_EQ(0, rv);

	ASSERT_EQ(0, strcmp(dir_list[0], "a"));
	ASSERT_EQ(0, strcmp(dir_list[1], "b"));
	ASSERT_EQ(0, strcmp(dir_list[2], "large_dir"));
	ASSERT_EQ(NULL, dir_list[3]);
	free(dir_list);

	rv = sqsh__archive_cleanup(&sqsh);
	ASSERT_EQ(0, rv);
}

UTEST(integration, test_tree_traversal) {
	int rv;
	struct SqshArchive sqsh = {0};
	struct SqshFile *file = NULL;
	struct SqshTreeTraversal *traversal = NULL;

	struct SqshConfig config = DEFAULT_CONFIG(TEST_SQUASHFS_IMAGE_LEN);
	config.archive_offset = 1010;
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	ASSERT_EQ(0, rv);

	file = sqsh_open(&sqsh, "/", &rv);
	ASSERT_EQ(0, rv);

	traversal = sqsh_tree_traversal_new(file, &rv);
	ASSERT_EQ(0, rv);

	bool has_next;
	const char *name;
	size_t size;

	has_next = sqsh_tree_traversal_next(traversal, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(true, has_next);
	name = sqsh_tree_traversal_name(traversal, &size);
	ASSERT_EQ(0, strcmp("", name));
	ASSERT_EQ(
			(enum SqshTreeTraversalState)
					SQSH_TREE_TRAVERSAL_STATE_DIRECTORY_BEGIN,
			sqsh_tree_traversal_state(traversal));

	has_next = sqsh_tree_traversal_next(traversal, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(true, has_next);
	name = sqsh_tree_traversal_name(traversal, &size);
	ASSERT_STRNEQ("a", name, size);
	ASSERT_EQ(
			(enum SqshTreeTraversalState)SQSH_TREE_TRAVERSAL_STATE_FILE,
			sqsh_tree_traversal_state(traversal));

	has_next = sqsh_tree_traversal_next(traversal, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(true, has_next);
	name = sqsh_tree_traversal_name(traversal, &size);
	ASSERT_STRNEQ("b", name, size);
	ASSERT_EQ(
			(enum SqshTreeTraversalState)SQSH_TREE_TRAVERSAL_STATE_FILE,
			sqsh_tree_traversal_state(traversal));

	has_next = sqsh_tree_traversal_next(traversal, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(true, has_next);
	name = sqsh_tree_traversal_name(traversal, &size);
	ASSERT_STRNEQ("large_dir", name, size);
	ASSERT_EQ(
			(enum SqshTreeTraversalState)
					SQSH_TREE_TRAVERSAL_STATE_DIRECTORY_BEGIN,
			sqsh_tree_traversal_state(traversal));

	while (sqsh_tree_traversal_next(traversal, &rv)) {
		name = sqsh_tree_traversal_name(traversal, &size);
		if (strncmp("link", name, size) == 0) {
			break;
		}
	}

	has_next = sqsh_tree_traversal_next(traversal, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(true, has_next);
	name = sqsh_tree_traversal_name(traversal, &size);
	ASSERT_STRNEQ("large_dir", name, size);
	ASSERT_EQ(
			(enum SqshTreeTraversalState)
					SQSH_TREE_TRAVERSAL_STATE_DIRECTORY_END,
			sqsh_tree_traversal_state(traversal));

	has_next = sqsh_tree_traversal_next(traversal, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(true, has_next);
	name = sqsh_tree_traversal_name(traversal, &size);
	ASSERT_STRNEQ("", name, size);
	ASSERT_EQ(
			(enum SqshTreeTraversalState)
					SQSH_TREE_TRAVERSAL_STATE_DIRECTORY_END,
			sqsh_tree_traversal_state(traversal));

	rv = sqsh_tree_traversal_free(traversal);
	ASSERT_EQ(0, rv);
	rv = sqsh_close(file);

	rv = sqsh__archive_cleanup(&sqsh);
	ASSERT_EQ(0, rv);
}

UTEST(integration, test_easy_traversal) {
	int rv;
	struct SqshArchive sqsh = {0};
	struct SqshFile *file = NULL;
	char **traversal = NULL;

	struct SqshConfig config = DEFAULT_CONFIG(TEST_SQUASHFS_IMAGE_LEN);
	config.archive_offset = 1010;
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	ASSERT_EQ(0, rv);

	traversal = sqsh_easy_tree_traversal(&sqsh, "/", &rv);
	ASSERT_EQ(0, rv);

	ASSERT_STREQ("a", traversal[0]);
	ASSERT_STREQ("b", traversal[1]);
	ASSERT_STREQ("large_dir", traversal[2]);
	ASSERT_STREQ("large_dir/1", traversal[3]);
	ASSERT_STREQ("large_dir/10", traversal[4]);

	free(traversal);
	rv = sqsh_close(file);

	rv = sqsh__archive_cleanup(&sqsh);
	ASSERT_EQ(0, rv);
}

UTEST(integration, test_traversal_zero_max_depth) {
	int rv;
	struct SqshArchive sqsh = {0};
	struct SqshFile *file = NULL;
	struct SqshTreeTraversal *traversal = NULL;

	struct SqshConfig config = DEFAULT_CONFIG(TEST_SQUASHFS_IMAGE_LEN);
	config.archive_offset = 1010;
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	ASSERT_EQ(0, rv);

	file = sqsh_open(&sqsh, "/", &rv);
	ASSERT_EQ(0, rv);

	traversal = sqsh_tree_traversal_new(file, &rv);
	ASSERT_EQ(0, rv);

	bool has_next;
	const char *name;
	size_t size;

	sqsh_tree_traversal_set_max_depth(traversal, 0);

	has_next = sqsh_tree_traversal_next(traversal, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(true, has_next);
	name = sqsh_tree_traversal_name(traversal, &size);
	ASSERT_EQ(0, strcmp("", name));
	ASSERT_EQ(
			(enum SqshTreeTraversalState)SQSH_TREE_TRAVERSAL_STATE_FILE,
			sqsh_tree_traversal_state(traversal));

	has_next = sqsh_tree_traversal_next(traversal, &rv);
	ASSERT_EQ(0, rv);
	ASSERT_EQ(false, has_next);

	rv = sqsh_tree_traversal_free(traversal);
	ASSERT_EQ(0, rv);

	rv = sqsh_close(file);

	rv = sqsh__archive_cleanup(&sqsh);
	ASSERT_EQ(0, rv);
}

UTEST(integration, mmap) {
	int rv;
	struct SqshArchive sqsh = {0};
	struct SqshFile *file = NULL;
	char **traversal = NULL;

	struct SqshConfig config = {
			.source_mapper = sqsh_mapper_impl_mmap,
			.archive_offset = 1010,
	};
	rv = sqsh__archive_init(&sqsh, (char *)INTEGRATION_PATH, &config);
	ASSERT_EQ(0, rv);

	traversal = sqsh_easy_tree_traversal(&sqsh, "/", &rv);
	ASSERT_EQ(0, rv);

	ASSERT_STREQ("a", traversal[0]);
	ASSERT_STREQ("b", traversal[1]);
	ASSERT_STREQ("large_dir", traversal[2]);
	ASSERT_STREQ("large_dir/1", traversal[3]);
	ASSERT_STREQ("large_dir/10", traversal[4]);

	free(traversal);
	rv = sqsh_close(file);

	rv = sqsh__archive_cleanup(&sqsh);
	ASSERT_EQ(0, rv);
}

UTEST(integration, copy_iterator_newly) {
	int rv;
	struct SqshArchive sqsh = {0};
	struct SqshFile *file = NULL;
	struct SqshFileIterator iter = {0};
	struct SqshFileIterator copy_iter = {0};

	struct SqshConfig config = DEFAULT_CONFIG(TEST_SQUASHFS_IMAGE_LEN);
	config.archive_offset = 1010;
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	ASSERT_EQ(0, rv);

	file = sqsh_open(&sqsh, "/b", &rv);
	ASSERT_EQ(0, rv);

	rv = sqsh__file_iterator_init(&iter, file);
	ASSERT_EQ(0, rv);

	rv = sqsh__file_iterator_copy(&copy_iter, &iter);
	ASSERT_EQ(0, rv);

	rv = sqsh_close(file);
	ASSERT_EQ(0, rv);

	rv = sqsh__archive_cleanup(&sqsh);
	ASSERT_EQ(0, rv);
}

UTEST(integration, copy_iterator_iterated) {
	int rv;
	struct SqshArchive sqsh = {0};
	struct SqshFile *file = NULL;
	struct SqshFileIterator iter = {0};
	struct SqshFileIterator copy_iter = {0};

	struct SqshConfig config = DEFAULT_CONFIG(TEST_SQUASHFS_IMAGE_LEN);
	config.archive_offset = 1010;
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	ASSERT_EQ(0, rv);

	file = sqsh_open(&sqsh, "/b", &rv);
	ASSERT_EQ(0, rv);

	rv = sqsh__file_iterator_init(&iter, file);
	ASSERT_EQ(0, rv);

	bool has_next = sqsh_file_iterator_next(&iter, 1, &rv);
	ASSERT_LE(0, rv);
	ASSERT_TRUE(has_next);
	rv = 0;

	rv = sqsh__file_iterator_copy(&copy_iter, &iter);
	ASSERT_EQ(0, rv);

	rv = sqsh__file_iterator_cleanup(&iter);
	ASSERT_EQ(0, rv);

	rv = sqsh__file_iterator_cleanup(&copy_iter);
	ASSERT_EQ(0, rv);

	rv = sqsh_close(file);
	ASSERT_EQ(0, rv);

	rv = sqsh__archive_cleanup(&sqsh);
	ASSERT_EQ(0, rv);
}

UTEST_MAIN()
