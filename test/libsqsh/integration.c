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
 * @file         integration.c
 */

#include "common.h"
#include <pthread.h>
#include <sqsh_archive_private.h>
#include <sqsh_directory_private.h>
#include <sqsh_easy.h>
#include <sqsh_file_private.h>
#include <sqsh_tree.h>
#include <sqsh_tree_private.h>
#include <testlib.h>

extern uint8_t squashfs_image[];
extern size_t squashfs_image_size;

#define TEST_SQUASHFS_IMAGE_LEN squashfs_image_size
#define TEST_SQUASHFS_IMAGE squashfs_image

static void
sqsh_empty(void) {
	int rv;
	struct SqshArchive sqsh = {0};
	struct SqshConfig config = DEFAULT_CONFIG(0);
	rv = sqsh__archive_init(&sqsh, NULL, &config);
	assert(rv == -SQSH_ERROR_SUPERBLOCK_TOO_SMALL);
}

static void
sqsh_get_nonexistant(void) {
	int rv;
	struct SqshArchive sqsh = {0};
	struct SqshPathResolver resolver = {0};

	struct SqshConfig config = DEFAULT_CONFIG(TEST_SQUASHFS_IMAGE_LEN);
	config.archive_offset = 1010;
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	assert(rv == 0);

	rv = sqsh__path_resolver_init(&resolver, &sqsh);
	assert(rv == 0);

	rv = sqsh_path_resolver_resolve(&resolver, "/nonexistant", false);
	assert(rv < 0);

	rv = sqsh__path_resolver_cleanup(&resolver);
	assert(rv == 0);

	rv = sqsh__archive_cleanup(&sqsh);
	assert(rv == 0);
}

static void
path_resolver(void) {
	int rv;
	struct SqshPathResolver resolver = {0};
	struct SqshArchive sqsh = {0};
	struct SqshFile *file;
	struct SqshConfig config = DEFAULT_CONFIG(TEST_SQUASHFS_IMAGE_LEN);
	config.archive_offset = 1010;
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	assert(rv == 0);

	rv = sqsh__path_resolver_init(&resolver, &sqsh);
	assert(rv == 0);

	rv = sqsh_path_resolver_resolve(&resolver, "/large_dir", false);
	assert(rv == 0);

	rv = sqsh_path_resolver_resolve(&resolver, "999", false);
	assert(rv == 0);

	file = sqsh_path_resolver_open_file(&resolver, &rv);
	assert(file != NULL);
	assert(rv == 0);

	rv = sqsh_close(file);
	assert(rv == 0);

	rv = sqsh__path_resolver_cleanup(&resolver);
	assert(rv == 0);

	rv = sqsh__archive_cleanup(&sqsh);
	assert(rv == 0);
}

static void
sqsh_ls(void) {
	int rv;
	char *name;
	struct SqshFile file = {0};
	struct SqshDirectoryIterator *iter = NULL;
	struct SqshArchive sqsh = {0};
	const struct SqshSuperblock *superblock;
	struct SqshConfig config = DEFAULT_CONFIG(TEST_SQUASHFS_IMAGE_LEN);
	config.archive_offset = 1010;
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	assert(rv == 0);

	superblock = sqsh_archive_superblock(&sqsh);
	rv = sqsh__file_init(
			&file, &sqsh, sqsh_superblock_inode_root_ref(superblock));
	assert(rv == 0);

	iter = sqsh_directory_iterator_new(&file, &rv);
	assert(iter != NULL);
	assert(rv == 0);

	bool has_next = sqsh_directory_iterator_next(iter, &rv);
	assert(rv == 0);
	assert(has_next);
	name = sqsh_directory_iterator_name_dup(iter);
	assert(name != NULL);
	assert(strcmp("a", name) == 0);
	free(name);

	has_next = sqsh_directory_iterator_next(iter, &rv);
	assert(rv >= 0);
	assert(has_next == true);
	name = sqsh_directory_iterator_name_dup(iter);
	assert(name != NULL);
	assert(strcmp("b", name) == 0);
	free(name);

	has_next = sqsh_directory_iterator_next(iter, &rv);
	assert(rv == 0);
	assert(has_next == true);
	name = sqsh_directory_iterator_name_dup(iter);
	assert(name != NULL);
	assert(strcmp("large_dir", name) == 0);
	free(name);

	has_next = sqsh_directory_iterator_next(iter, &rv);
	// End of file list
	assert(rv == 0);
	assert(has_next == false);

	rv = sqsh_directory_iterator_free(iter);
	assert(rv == 0);

	rv = sqsh__file_cleanup(&file);
	assert(rv == 0);

	rv = sqsh__archive_cleanup(&sqsh);
	assert(rv == 0);
}

static void
sqsh_read_content(void) {
	int rv;
	char *data;
	struct SqshArchive archive = {0};
	struct SqshConfig config = DEFAULT_CONFIG(TEST_SQUASHFS_IMAGE_LEN);
	config.archive_offset = 1010;
	rv = sqsh__archive_init(&archive, (char *)TEST_SQUASHFS_IMAGE, &config);
	assert(rv == 0);

	data = (char *)sqsh_easy_file_content(&archive, "/a", NULL);
	assert(strcmp(data, "a\n") == 0);
	free(data);

	rv = sqsh__archive_cleanup(&archive);
	assert(rv == 0);
}

static void
sqsh_cat_fragment(void) {
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
	assert(rv == 0);

	rv = sqsh__path_resolver_init(&resolver, &sqsh);
	assert(rv == 0);

	rv = sqsh_path_resolver_resolve(&resolver, "a", false);
	assert(rv == 0);

	file = sqsh_path_resolver_open_file(&resolver, &rv);
	assert(file != NULL);
	assert(rv == 0);

	rv = sqsh__file_reader_init(&reader, file);
	assert(rv == 0);

	size = sqsh_file_size(file);
	assert(size == 2);

	rv = sqsh_file_reader_advance(&reader, 0, size);
	assert(rv == 0);

	data = sqsh_file_reader_data(&reader);
	assert(memcmp(data, "a\n", size) == 0);

	rv = sqsh__file_reader_cleanup(&reader);
	assert(rv == 0);

	rv = sqsh_close(file);
	assert(rv == 0);

	rv = sqsh__path_resolver_cleanup(&resolver);
	assert(rv == 0);

	rv = sqsh__archive_cleanup(&sqsh);
	assert(rv == 0);
}

static void
sqsh_cat_datablock_and_fragment(void) {
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
	assert(rv == 0);

	rv = sqsh__path_resolver_init(&resolver, &sqsh);
	assert(rv == 0);

	rv = sqsh_path_resolver_resolve(&resolver, "b", false);
	assert(rv == 0);

	file = sqsh_path_resolver_open_file(&resolver, &rv);
	assert(file != NULL);
	assert(rv == 0);

	rv = sqsh__file_reader_init(&reader, file);
	assert(rv == 0);

	size = sqsh_file_size(file);
	assert(size == 1050000);

	rv = sqsh_file_reader_advance(&reader, 0, size);
	assert(rv == 0);
	assert(size == sqsh_file_reader_size(&reader));

	data = sqsh_file_reader_data(&reader);
	for (sqsh_index_t i = 0; i < size; i++) {
		assert(data[i] == 'b');
	}

	rv = sqsh__file_reader_cleanup(&reader);
	assert(rv == 0);

	rv = sqsh_close(file);
	assert(rv == 0);

	rv = sqsh__path_resolver_cleanup(&resolver);
	assert(rv == 0);

	rv = sqsh__archive_cleanup(&sqsh);
	assert(rv == 0);
}

static void
sqsh_cat_size_overflow(void) {
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
	assert(rv == 0);

	rv = sqsh__path_resolver_init(&resolver, &sqsh);
	assert(rv == 0);

	rv = sqsh_path_resolver_resolve(&resolver, "b", false);
	assert(rv == 0);

	file = sqsh_path_resolver_open_file(&resolver, &rv);
	assert(file != NULL);
	assert(rv == 0);

	rv = sqsh__file_reader_init(&reader, file);
	assert(rv == 0);
	size = sqsh_file_size(file);
	assert(size == 1050000);

	rv = sqsh_file_reader_advance(&reader, 0, size + 4096);
	assert(rv == -SQSH_ERROR_OUT_OF_BOUNDS);

	rv = sqsh__file_reader_cleanup(&reader);
	assert(rv == 0);

	rv = sqsh_close(file);
	assert(rv == 0);

	rv = sqsh__path_resolver_cleanup(&resolver);
	assert(rv == 0);

	rv = sqsh__archive_cleanup(&sqsh);
	assert(rv == 0);
}

static void
sqsh_test_uid_and_gid(void) {
	int rv;
	uint32_t uid, gid;
	struct SqshFile file = {0};
	struct SqshArchive sqsh = {0};
	const struct SqshSuperblock *superblock;
	const struct SqshConfig config = {
			.source_mapper = sqsh_mapper_impl_static,
			.source_size = TEST_SQUASHFS_IMAGE_LEN,
			.archive_offset = 1010,
	};
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	assert(rv == 0);

	superblock = sqsh_archive_superblock(&sqsh);
	rv = sqsh__file_init(
			&file, &sqsh, sqsh_superblock_inode_root_ref(superblock));
	assert(rv == 0);

	uid = sqsh_file_uid(&file);
	assert(uid == 2020);
	gid = sqsh_file_gid(&file);
	assert(gid == 202020);

	rv = sqsh__file_cleanup(&file);
	assert(rv == 0);

	rv = sqsh__archive_cleanup(&sqsh);
	assert(rv == 0);
}

static void
sqsh_test_extended_dir(void) {
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
	assert(rv == 0);

	rv = sqsh__path_resolver_init(&resolver, &sqsh);
	assert(rv == 0);

	rv = sqsh_path_resolver_resolve(&resolver, "/large_dir/999", false);
	assert(rv == 0);

	file = sqsh_path_resolver_open_file(&resolver, &rv);
	assert(file != NULL);

	rv = sqsh_close(file);
	assert(rv == 0);

	rv = sqsh__path_resolver_cleanup(&resolver);

	rv = sqsh__archive_cleanup(&sqsh);
	assert(rv == 0);
}

#if 0
static void
sqsh_test_xattr(void) {
	const char *expected_value = "1234567891234567891234567890001234567890";
	int rv;
	char *name, *value;
	struct SqshFile *file = NULL;
	struct SqshFile *entry_file = NULL;
	struct SqshDirectoryIterator *dir_iter = NULL;
	struct SqshXattrIterator *xattr_iter = NULL;
	struct SqshArchive sqsh = {0};
	const struct SqshSuperblock *superblock;
	struct SqshConfig config = {
			.source_mapper = sqsh_mapper_impl_static,
			.source_size = TEST_SQUASHFS_IMAGE_LEN,
			.archive_offset = 1010,
	};
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	assert(rv == 0);

	superblock = sqsh_archive_superblock(&sqsh);
	file = sqsh_file_new(
			&sqsh, sqsh_superblock_inode_root_ref(superblock), &rv);
	assert(rv == 0);

	xattr_iter = sqsh_xattr_iterator_new(file, &rv);
	assert(xattr_iter != NULL);
	assert(rv == 0);
	rv = sqsh_xattr_iterator_next(xattr_iter);
	assert(rv == 0);
	rv = sqsh_xattr_iterator_free(xattr_iter);
	assert(rv == 0);

	dir_iter = sqsh_directory_iterator_new(file, &rv);
	assert(dir_iter != NULL);
	assert(rv == 0);

	rv = sqsh_directory_iterator_next(dir_iter);
	assert(rv > 0);
	name = sqsh_directory_iterator_name_dup(dir_iter);
	assert(name != NULL);
	assert(strcmp("a", name) == 0);
	free(name);
	entry_file = sqsh_directory_iterator_open_file(dir_iter, &rv);
	assert(rv == 0);
	xattr_iter = sqsh_xattr_iterator_new(entry_file, &rv);
	assert(xattr_iter != NULL);
	assert(rv == 0);
	rv = sqsh_xattr_iterator_next(xattr_iter);
	assert(rv > 0);
	assert(sqsh_xattr_iterator_is_indirect(xattr_iter) == false);
	name = sqsh_xattr_iterator_fullname_dup(xattr_iter);
	assert(name != NULL);
	assert(strcmp("user.foo", name) == 0);
	free(name);
	value = sqsh_xattr_iterator_value_dup(xattr_iter);
	assert(value != NULL);
	assert(strcmp(expected_value, value) == 0);
	free(value);
	rv = sqsh_xattr_iterator_next(xattr_iter);
	assert(rv == 0);
	rv = sqsh_xattr_iterator_free(xattr_iter);
	assert(rv == 0);
	rv = sqsh_close(entry_file);
	assert(rv == 0);

	rv = sqsh_directory_iterator_next(dir_iter);
	assert(rv >= 0);
	name = sqsh_directory_iterator_name_dup(dir_iter);
	assert(name != NULL);
	assert(strcmp("b", name) == 0);
	free(name);
	entry_file = sqsh_directory_iterator_open_file(dir_iter, &rv);
	assert(rv == 0);
	xattr_iter = sqsh_xattr_iterator_new(entry_file, &rv);
	assert(xattr_iter != NULL);
	assert(rv == 0);
	rv = sqsh_xattr_iterator_next(xattr_iter);
	assert(rv > 0);
	assert(sqsh_xattr_iterator_is_indirect(xattr_iter) == true);
	name = sqsh_xattr_iterator_fullname_dup(xattr_iter);
	assert(name != NULL);
	assert(strcmp("user.bar", name) == 0);
	free(name);
	value = sqsh_xattr_iterator_value_dup(xattr_iter);
	assert(value != NULL);
	assert(strcmp(expected_value, value) == 0);
	free(value);
	rv = sqsh_xattr_iterator_next(xattr_iter);
	assert(rv == 0);
	rv = sqsh_xattr_iterator_free(xattr_iter);
	assert(rv == 0);
	rv = sqsh_close(entry_file);
	assert(rv == 0);

	rv = sqsh_directory_iterator_free(dir_iter);
	assert(rv == 0);

	rv = sqsh_file_cleanup(file);
	assert(rv == 0);

	rv = sqsh__archive_cleanup(&sqsh);
	assert(rv == 0);
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
		rv = sqsh_file_reader_advance(reader, 0, size);
		assert(rv == 0);
		sqsh_file_reader_free(reader);
	}

	sqsh_close(file);

	return 0;
}
static void
multithreaded(void) {
	int rv;
	pthread_t threads[16] = {0};
	struct SqshArchive sqsh = {0};

	struct SqshConfig config = DEFAULT_CONFIG(TEST_SQUASHFS_IMAGE_LEN);
	config.archive_offset = 1010;
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	assert(rv == 0);

	const struct SqshSuperblock *superblock = sqsh_archive_superblock(&sqsh);
	struct Walker resolver = {
			.sqsh = &sqsh,
			.inode_number = sqsh_superblock_inode_root_ref(superblock),
	};
	for (unsigned long i = 0; i < LENGTH(threads); i++) {
		rv = pthread_create(
				&threads[i], NULL, multithreaded_resolver, &resolver);
		assert(rv == 0);
	}

	for (unsigned long i = 0; i < LENGTH(threads); i++) {
		rv = pthread_join(threads[i], NULL);
		assert(rv == 0);
	}

	rv = sqsh__archive_cleanup(&sqsh);
	assert(rv == 0);
}
static void
test_follow_symlink(void) {
	int rv;
	struct SqshArchive sqsh = {0};

	struct SqshConfig config = DEFAULT_CONFIG(TEST_SQUASHFS_IMAGE_LEN);
	config.archive_offset = 1010;
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	assert(rv == 0);

	char **dir_list = sqsh_easy_directory_list(
			&sqsh, "/large_dir/link/large_dir/link", &rv);
	assert(rv == 0);

	assert(strcmp(dir_list[0], "a") == 0);
	assert(strcmp(dir_list[1], "b") == 0);
	assert(strcmp(dir_list[2], "large_dir") == 0);
	assert(dir_list[3] == NULL);
	free(dir_list);

	rv = sqsh__archive_cleanup(&sqsh);
	assert(rv == 0);
}
static void
test_tree_traversal(void) {
	int rv;
	struct SqshArchive sqsh = {0};
	struct SqshFile *file = NULL;
	struct SqshTreeTraversal *traversal = NULL;

	struct SqshConfig config = DEFAULT_CONFIG(TEST_SQUASHFS_IMAGE_LEN);
	config.archive_offset = 1010;
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	assert(rv == 0);

	file = sqsh_open(&sqsh, "/", &rv);
	assert(rv == 0);

	traversal = sqsh_tree_traversal_new(file, &rv);
	assert(rv == 0);

	rv = sqsh_tree_traversal_free(traversal);
	assert(rv == 0);
	rv = sqsh_close(file);

	rv = sqsh__archive_cleanup(&sqsh);
	assert(rv == 0);
}
static void
test_read_end_of_file(void) {
	int rv;
	struct SqshArchive sqsh = {0};
	struct SqshFile *file = NULL;
	struct SqshFileReader *reader = NULL;

	struct SqshConfig config = DEFAULT_CONFIG(TEST_SQUASHFS_IMAGE_LEN);
	config.archive_offset = 1010;
	rv = sqsh__archive_init(&sqsh, (char *)TEST_SQUASHFS_IMAGE, &config);
	assert(rv == 0);

	file = sqsh_open(&sqsh, "/b", &rv);
	assert(rv == 0);

	reader = sqsh_file_reader_new(file, &rv);

	uint64_t size = sqsh_file_size(file);

	rv = sqsh_file_reader_advance(reader, size, 0);
	assert(rv == 0);
	assert(sqsh_file_reader_size(reader) == 0);
	assert(sqsh_file_reader_data(reader) != NULL);

	rv = sqsh_file_reader_advance(reader, 0, 1);
	assert(rv == -SQSH_ERROR_OUT_OF_BOUNDS);

	rv = sqsh_close(file);
	assert(rv == 0);
}

DECLARE_TESTS
TEST(sqsh_empty)
TEST(sqsh_ls)
TEST(path_resolver)
TEST(sqsh_get_nonexistant)
TEST(sqsh_read_content)
TEST(sqsh_cat_fragment)
TEST(sqsh_cat_datablock_and_fragment)
TEST(sqsh_cat_size_overflow)
TEST(sqsh_test_uid_and_gid)
TEST(sqsh_test_extended_dir)
// TEST(sqsh_test_xattr);
TEST(multithreaded)
TEST(test_follow_symlink)
TEST(test_tree_traversal)
TEST(test_read_end_of_file)
END_TESTS
