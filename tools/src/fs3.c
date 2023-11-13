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
 * @author       Enno Boland (mail@eboland.de)
 * @file         fs3.c
 */

#define FUSE_USE_VERSION 35

#include <sqshtools_fs_common.h>

#include <errno.h>
#include <fuse_lowlevel.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

struct Context {
	struct SqshArchive *archive;
};

struct FsDirHandle {
	struct SqshFile *file;
	struct SqshDirectoryIterator *iterator;
	char *current_name;
	struct stat current_stat;
};

static struct Context context = {0};
struct fuse_cmdline_opts fuse_options = {0};
struct SqshfsOptions options = {0};

static uint64_t
fs_common_context_inode_ref(fuse_ino_t fuse_inode, int *err) {
	int rv = 0;
	uint64_t sqsh_inode = fs_common_inode_sqsh_from_ino(fuse_inode);
	struct SqshArchive *archive = context.archive;
	struct SqshInodeMap *inode_map;
	const struct SqshSuperblock *superblock = sqsh_archive_superblock(archive);

	if (fuse_inode == FUSE_ROOT_ID) {
		return sqsh_superblock_inode_root_ref(superblock);
	}

	rv = sqsh_archive_inode_map(archive, &inode_map);
	if (rv < 0) {
		*err = rv;
		return 0;
	}

	return sqsh_inode_map_get2(inode_map, sqsh_inode, err);
}

static struct FsDirHandle *
get_dir_handle(struct fuse_file_info *fi) {
	return (struct FsDirHandle *)(uintptr_t)fi->fh;
}

static void
fs_init(void *userdata, struct fuse_conn_info *conn) {
	(void)userdata;

	if (conn->capable & FUSE_CAP_PARALLEL_DIROPS) {
		conn->want = FUSE_CAP_PARALLEL_DIROPS;
	}
}

static struct SqshFile *
fs_file_open(fuse_ino_t ino, int *err) {
	const uint64_t inode_ref = fs_common_context_inode_ref(ino, err);
	if (*err < 0) {
		return NULL;
	}
	return sqsh_open_by_ref(context.archive, inode_ref, err);
}

static void
fs_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
	(void)fi;
	int rv = 0;
	struct SqshFile *file = NULL;
	const struct SqshSuperblock *superblock =
			sqsh_archive_superblock(context.archive);

	file = fs_file_open(ino, &rv);
	if (rv < 0) {
		fuse_reply_err(req, -fs_common_map_err(rv));
		goto out;
	}

	struct stat stbuf = {0};
	fs_common_getattr(file, superblock, &stbuf);

	fuse_reply_attr(req, &stbuf, 1.0);
out:
	sqsh_close(file);
}

static void
fs_lookup(fuse_req_t req, fuse_ino_t parent, const char *name) {
	int rv = 0;
	struct SqshDirectoryIterator *iterator = NULL;
	struct SqshFile *parent_dir = NULL;
	struct SqshFile *file = NULL;
	const struct SqshSuperblock *superblock =
			sqsh_archive_superblock(context.archive);

	parent_dir = fs_file_open(parent, &rv);
	if (rv < 0) {
		fuse_reply_err(req, -fs_common_map_err(rv));
		goto out;
	}

	iterator = sqsh_directory_iterator_new(parent_dir, &rv);

	rv = sqsh_directory_iterator_lookup(iterator, name, strlen(name));
	if (rv < 0) {
		fuse_reply_err(req, -fs_common_map_err(rv));
		goto out;
	}

	const uint64_t inode_ref = sqsh_directory_iterator_inode_ref(iterator);
	file = sqsh_open_by_ref(context.archive, inode_ref, &rv);
	if (rv < 0) {
		fuse_reply_err(req, -fs_common_map_err(rv));
		goto out;
	}

	uint32_t inode_number = sqsh_file_inode(file);
	if (inode_number > sqsh_superblock_inode_count(superblock)) {
		fuse_reply_err(req, EIO);
		goto out;
	}

	struct fuse_entry_param entry = {
			.ino = fs_common_inode_sqsh_to_ino(inode_number),
			.attr_timeout = 1.0,
			.entry_timeout = 1.0,
			.generation = 1,
	};
	fs_common_getattr(file, NULL, &entry.attr);

	fuse_reply_entry(req, &entry);

out:
	sqsh_close(file);
	sqsh_directory_iterator_free(iterator);
	sqsh_close(parent_dir);
}

static void
fs_access(fuse_req_t req, fuse_ino_t ino, int mask) {
	int rv = 0;
	struct SqshFile *file = NULL;

	file = fs_file_open(ino, &rv);
	if (rv < 0) {
		fuse_reply_err(req, -fs_common_map_err(rv));
		goto out;
	}

	const uint16_t permission = sqsh_file_permission(file);
	fuse_reply_err(req, permission & mask ? 0 : EACCES);

out:
	sqsh_close(file);
}

static void
fs_readlink(fuse_req_t req, fuse_ino_t ino) {
	int rv = 0;
	struct SqshFile *file = NULL;

	file = fs_file_open(ino, &rv);
	if (rv < 0) {
		fuse_reply_err(req, -fs_common_map_err(rv));
		goto out;
	}
	if (sqsh_file_type(file) != SQSH_FILE_TYPE_SYMLINK) {
		fuse_reply_err(req, EINVAL);
		goto out;
	}

	char *symlink = NULL;
	// fuse expects a null terminated string. Duplicate the symlink is the
	// easiest way to do this.
	symlink = sqsh_file_symlink_dup(file);
	if (symlink == NULL) {
		fuse_reply_err(req, ENOMEM);
		goto out;
	}
	fuse_reply_readlink(req, symlink);
	free(symlink);

out:
	sqsh_close(file);
}

static void
fs_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
	int rv = 0;
	struct FsDirHandle *handle = calloc(1, sizeof(*handle));

	if (handle == NULL) {
		fuse_reply_err(req, ENOMEM);
		goto out;
	}

	handle->file = fs_file_open(ino, &rv);
	if (rv < 0) {
		fuse_reply_err(req, -fs_common_map_err(rv));
		goto out;
	}
	handle->iterator = sqsh_directory_iterator_new(handle->file, &rv);
	if (rv < 0) {
		fuse_reply_err(req, -fs_common_map_err(rv));
		goto out;
	}

	fi->fh = (uintptr_t)handle;
	fuse_reply_open(req, fi);
out:
	if (rv < 0) {
		sqsh_close(handle->file);
		sqsh_directory_iterator_free(handle->iterator);
		free(handle);
	}
}

static void
fs_readdir(
		fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset,
		struct fuse_file_info *fi) {
	(void)ino;

	int rv = 0;
	struct FsDirHandle *handle = get_dir_handle(fi);
	char buf[size];

	bool has_next = sqsh_directory_iterator_next(handle->iterator, &rv);
	if (rv < 0) {
		fuse_reply_err(req, -fs_common_map_err(rv));
		goto out;
	} else if (has_next == false) {
		fuse_reply_buf(req, NULL, 0);
		goto out;
	}

	handle->current_stat.st_ino = fs_common_inode_sqsh_to_ino(
			sqsh_directory_iterator_inode(handle->iterator));

	handle->current_stat.st_mode = fs_common_mode_type(
			sqsh_directory_iterator_file_type(handle->iterator));
	handle->current_name = sqsh_directory_iterator_name_dup(handle->iterator);
	if (handle->current_name == NULL) {
		fuse_reply_err(req, ENOMEM);
		goto out;
	}
	size_t result_size = fuse_add_direntry(
			req, buf, size, handle->current_name, &handle->current_stat,
			offset);
	free(handle->current_name);

	fuse_reply_buf(req, buf, result_size);

out:
	return;
}

static void
fs_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
	(void)ino;
	struct FsDirHandle *handle = get_dir_handle(fi);

	sqsh_close(handle->file);
	sqsh_directory_iterator_free(handle->iterator);
	free(handle);
	fuse_reply_err(req, 0);
}

static void
fs_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
	struct SqshFile *file = NULL;

	int rv = 0;

	file = fs_file_open(ino, &rv);
	if (rv < 0) {
		fuse_reply_err(req, EIO);
		goto out;
	}
	fi->fh = (uintptr_t)file;
	file = NULL;
	fuse_reply_open(req, fi);
out:
	sqsh_close(file);
}

static void
fs_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
	(void)ino;
	struct SqshFile *file = (void *)fi->fh;

	sqsh_close(file);
	fuse_reply_err(req, 0);
}

static void
fs_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset,
		struct fuse_file_info *fi) {
	(void)ino;
	struct SqshFile *file = (void *)fi->fh;
	int rv = 0;
	struct SqshFileReader *reader = NULL;

	rv = fs_common_read(&reader, file, offset, size);
	if (rv < 0) {
		goto out;
	}

	const uint8_t *data = sqsh_file_reader_data(reader);
	const size_t data_size = sqsh_file_reader_size(reader);
	fuse_reply_buf(req, (const char *)data, data_size);

out:
	if (rv < 0) {
		fuse_reply_err(req, -fs_common_map_err(rv));
	}
	sqsh_file_reader_free(reader);
}

static void
fs_getxattr(fuse_req_t req, fuse_ino_t ino, const char *name, size_t size) {
	int rv = 0;
	struct SqshFile *file = NULL;
	struct SqshXattrIterator *iterator = NULL;
	const char *value_ptr = NULL;
	size_t value_size;

	file = fs_file_open(ino, &rv);
	if (rv < 0) {
		fuse_reply_err(req, EIO);
		goto out;
	}

	iterator = sqsh_xattr_iterator_new(file, &rv);
	if (rv < 0) {
		fuse_reply_err(req, EIO);
		goto out;
	}

	rv = sqsh_xattr_iterator_lookup(iterator, name);
	if (rv < 0) {
		fuse_reply_err(req, fs_common_map_err(rv));
		goto out;
	}

	value_ptr = sqsh_xattr_iterator_value(iterator);
	value_size = sqsh_xattr_iterator_value_size(iterator);

	if (size == 0) {
		fuse_reply_xattr(req, value_size);
	} else if (size < value_size) {
		fuse_reply_err(req, ERANGE);
	} else {
		fuse_reply_buf(req, value_ptr, value_size);
	}

out:
	sqsh_xattr_iterator_free(iterator);
	sqsh_close(file);
}

static void
fs_listxattr(fuse_req_t req, fuse_ino_t ino, size_t size) {
	(void)req;
	(void)ino;
	(void)size;
	int rv = 0;
	struct SqshFile *file = NULL;
	struct SqshXattrIterator *iterator = NULL;
	const char *prefix, *name = NULL;
	char buf[size];
	size_t buf_size = 0;
	char *p;

	file = fs_file_open(ino, &rv);
	if (rv < 0) {
		fuse_reply_err(req, EIO);
		goto out;
	}

	iterator = sqsh_xattr_iterator_new(file, &rv);
	if (rv < 0) {
		fuse_reply_err(req, EIO);
		goto out;
	}

	p = buf;
	while (sqsh_xattr_iterator_next(iterator, &rv)) {
		prefix = sqsh_xattr_iterator_prefix(iterator);
		if (prefix == NULL) {
			fuse_reply_err(req, EIO);
			goto out;
		}
		name = sqsh_xattr_iterator_name(iterator);
		if (name == NULL) {
			fuse_reply_err(req, EIO);
			goto out;
		}

		size_t prefix_size = sqsh_xattr_iterator_prefix_size(iterator);
		buf_size += prefix_size;
		if (buf_size < size) {
			memcpy(p, prefix, prefix_size);
			p = &buf[buf_size];
		}

		size_t name_size = sqsh_xattr_iterator_name_size(iterator);
		buf_size += name_size + 1;
		if (buf_size < size) {
			memcpy(p, name, name_size);
			p[name_size] = '\0';
			p = &buf[buf_size];
		}
	}
	if (rv < 0) {
		fuse_reply_err(req, EIO);
		goto out;
	}
	if (size == 0) {
		fuse_reply_xattr(req, buf_size);
	} else {
		fuse_reply_buf(req, buf, buf_size);
	}
out:
	sqsh_xattr_iterator_free(iterator);
	sqsh_close(file);
}

static const struct fuse_lowlevel_ops fs_oper = {
		.init = fs_init,
		.lookup = fs_lookup,
		.access = fs_access,
		.getattr = fs_getattr,
		.readlink = fs_readlink,
		.opendir = fs_opendir,
		.readdir = fs_readdir,
		.releasedir = fs_releasedir,
		.open = fs_open,
		.release = fs_release,
		.read = fs_read,
		.getxattr = fs_getxattr,
		.listxattr = fs_listxattr,
		//.lseek = fs_lseek,
};

static int
opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs) {
	(void)outargs;
	struct SqshfsOptions *fs_common_options = data;
	if (key == FUSE_OPT_KEY_NONOPT && fs_common_options->archive == NULL) {
		fs_common_options->archive = arg;
		return 0;
	}
	return 1;
}

static int
parse_args(struct fuse_args *args) {
	int rv = 0;

	rv = fuse_opt_parse(args, &options, fs_common_opts, opt_proc);
	if (rv != 0) {
		return rv;
	}

	rv = fuse_parse_cmdline(args, &fuse_options);
	if (rv != 0) {
		return rv;
	}

	if (options.archive == NULL) {
		fs_common_usage(args->argv[0]);
		return -1;
	}

	return 0;
}

int
main(int argc, char *argv[]) {
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse_session *fuse_session = NULL;
	struct fuse_loop_config config = {0};
	int rv = EXIT_SUCCESS;

	rv = parse_args(&args);
	if (rv != 0) {
		rv = EXIT_FAILURE;
		goto out;
	}

	context.archive = open_archive(options.archive, options.offset, &rv);
	if (rv < 0) {
		sqsh_perror(rv, options.archive);
		rv = EXIT_FAILURE;
		goto out;
	}

	fuse_session = fuse_session_new(&args, &fs_oper, sizeof(fs_oper), NULL);
	if (fuse_session == NULL) {
		goto out;
	}

	if (fuse_set_signal_handlers(fuse_session) != 0) {
		rv = EXIT_FAILURE;
		goto out;
	}

	if (fuse_session_mount(fuse_session, fuse_options.mountpoint) != 0) {
		rv = EXIT_FAILURE;
		goto out;
	}

	if (fuse_daemonize(fuse_options.foreground) != 0) {
		rv = EXIT_FAILURE;
		goto out;
	}

	if (fuse_options.singlethread)
		rv = fuse_session_loop(fuse_session);
	else {
		config.clone_fd = fuse_options.clone_fd;
		config.max_idle_threads = fuse_options.max_idle_threads;
		rv = fuse_session_loop_mt(fuse_session, &config);
	}

	fuse_session_unmount(fuse_session);

out:
	if (fuse_session != NULL) {
		fuse_remove_signal_handlers(fuse_session);
		fuse_session_destroy(fuse_session);
	}
	free(fuse_options.mountpoint);
	fuse_opt_free_args(&args);
	sqsh_archive_close(context.archive);

	return rv;
}
