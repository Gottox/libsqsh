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
 * @file         cat.c
 */

#define FUSE_USE_VERSION 28

#include <sqshtools_fs_common.h>

#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef ENODATA
// On OpenBSD ENODATA is not defined, so lets return ENOATTR instead.
#	define ENODATA ENOATTR
#endif

struct Context {
	struct SqshArchive *archive;
	struct fuse_chan *fuse_ch;
};

static struct Context context = {0};
static struct SqshfsOptions options = {0};

static struct SqshFile *
fs_file_open(const char *path, int *err) {
	return sqsh_open(context.archive, path, err);
}

static int
fs_access(const char *path, int mask) {
	int rv = 0;

	struct SqshFile *file = fs_file_open(path, &rv);
	if (rv < 0) {
		return rv;
	}

	const uint16_t permission = sqsh_file_permission(file);

	sqsh_close(file);
	return permission & mask ? 0 : -EACCES;
}

static int
fs_getattr(const char *path, struct stat *stbuf) {
	int rv = 0;

	struct SqshFile *file = sqsh_open(context.archive, path, &rv);
	if (rv < 0) {
		return rv;
	}

	fs_common_getattr(file, NULL, stbuf);

	sqsh_close(file);
	return rv;
}

static int
fs_readdir_item(
		void *buf, struct SqshDirectoryIterator *iterator,
		fuse_fill_dir_t filler) {
	struct SqshFile *file = NULL;
	int rv = 0;
	char *name = NULL;
	struct stat stbuf = {0};

	name = sqsh_directory_iterator_name_dup(iterator);
	if (name == NULL) {
		rv = -SQSH_ERROR_MALLOC_FAILED;
		goto out;
	}
	file = sqsh_directory_iterator_open_file(iterator, &rv);
	if (rv < 0) {
		goto out;
	}
	fs_common_getattr(file, NULL, &stbuf);

	filler(buf, name, &stbuf, 0);

out:
	free(name);
	sqsh_close(file);
	return fs_common_map_err(rv);
}

static int
fs_readdir(
		const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
		struct fuse_file_info *fi) {
	(void)offset;
	(void)fi;
	int rv = 0;

	struct SqshFile *file = sqsh_open(context.archive, path, &rv);
	if (rv < 0) {
		return rv;
	}

	struct SqshDirectoryIterator *iterator =
			sqsh_directory_iterator_new(file, &rv);
	if (rv < 0) {
		goto out;
	}
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	while (sqsh_directory_iterator_next(iterator, &rv)) {
		rv = fs_readdir_item(buf, iterator, filler);
		if (rv < 0) {
			break;
		}
	}
	if (rv < 0) {
		goto out;
	}

out:
	sqsh_directory_iterator_free(iterator);
	sqsh_close(file);
	return 0;
}

static int
fs_open(const char *path, struct fuse_file_info *fi) {
	int rv = 0;

	struct SqshFile *file = sqsh_open(context.archive, path, &rv);
	if (rv < 0) {
		goto out;
	}

	fi->fh = (uint64_t)file;

out:
	if (rv < 0) {
		sqsh_close(file);
	}
	return fs_common_map_err(rv);
}

static int
fs_read(const char *path, char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi) {
	(void)path;
	int rv = 0;
	struct SqshFile *file = (struct SqshFile *)fi->fh;
	struct SqshFileReader *reader = NULL;

	rv = fs_common_read(&reader, file, offset, size);
	if (rv < 0) {
		goto out;
	}

	const uint8_t *data = sqsh_file_reader_data(reader);
	rv = sqsh_file_reader_size(reader);
	size = rv;
	memcpy(buf, data, size);

out:
	sqsh_file_reader_free(reader);

	return fs_common_map_err(rv);
}

static int
fs_release(const char *path, struct fuse_file_info *fi) {
	(void)path;
	struct SqshFile *file = (struct SqshFile *)fi->fh;

	sqsh_close(file);
	return 0;
}

static int
fs_readlink(const char *path, char *buf, size_t size) {
	int rv = 0;
	const char *link = NULL;
	size_t link_len = 0;

	struct SqshFile *file = fs_file_open(path, &rv);
	if (rv < 0) {
		goto out;
	}

	link = sqsh_file_symlink(file);
	link_len = sqsh_file_symlink_size(file);
	if (link == NULL) {
		rv = -SQSH_ERROR_MALLOC_FAILED;
		goto out;
	}

	if (link_len > size) {
		link_len = size;
	}
	memcpy(buf, link, link_len);
	buf[size - 1] = '\0';

out:
	sqsh_close(file);
	return fs_common_map_err(rv);
}

static int
fs_listxattr(const char *path, char *buf, size_t size) {
	int rv = 0;
	int pos = 0;
	struct SqshXattrIterator *iterator = NULL;

	struct SqshFile *file = fs_file_open(path, &rv);
	if (rv < 0) {
		goto out;
	}

	iterator = sqsh_xattr_iterator_new(file, &rv);
	while (sqsh_xattr_iterator_next(iterator, &rv) > 0) {
		const char *prefix = sqsh_xattr_iterator_prefix(iterator);
		size_t prefix_len = sqsh_xattr_iterator_prefix_size(iterator);
		const char *name = sqsh_xattr_iterator_name(iterator);
		size_t name_len = sqsh_xattr_iterator_name_size(iterator);
		if (pos + name_len + prefix_len + 1 > size) {
			rv = -ERANGE;
			goto out;
		}
		memcpy(&buf[pos], prefix, prefix_len);
		pos += prefix_len;
		memcpy(&buf[pos], name, name_len);
		pos += name_len;
		buf[pos] = '\0';
		pos++;
	}
	if (rv < 0) {
		goto out;
	}

	rv = pos;
out:
	sqsh_xattr_iterator_free(iterator);
	sqsh_close(file);
	return rv;
}

static int
fs_getxattr(const char *path, const char *name, char *buf, size_t size) {
	int rv = 0;
	struct SqshXattrIterator *iterator = NULL;

	struct SqshFile *file = fs_file_open(path, &rv);
	if (rv < 0) {
		goto out;
	}

	iterator = sqsh_xattr_iterator_new(file, &rv);
	if (rv < 0) {
		goto out;
	}

	rv = sqsh_xattr_iterator_lookup(iterator, name);
	if (rv < 0) {
		goto out;
	}

	const char *value = sqsh_xattr_iterator_value(iterator);
	size_t value_len = sqsh_xattr_iterator_value_size(iterator);
	if (value_len > size) {
		rv = SQSH_ERROR_OUT_OF_BOUNDS;
		goto out;
	}

	memcpy(buf, value, value_len);
	buf[value_len] = '\0';
	rv = value_len;

out:
	sqsh_xattr_iterator_free(iterator);
	sqsh_close(file);
	return fs_common_map_err(rv);
}

#ifdef __APPLE__
static int
fs_mac_getxattr(
		const char *path, const char *name, char *buf, size_t size,
		uint32_t position) {
	(void)position;
	return fs_getxattr(path, name, buf, size);
}
#endif

static struct fuse_operations fs_oper = {
		.access = fs_access,
		.getattr = fs_getattr,
		.readdir = fs_readdir,
		.open = fs_open,
		.read = fs_read,
		.release = fs_release,
		.readlink = fs_readlink,
		.listxattr = fs_listxattr,
#ifdef __APPLE__
		// Dear macfuse developers, this ifdef is on you. I really try very hard
		// to keep my source clean, but this is on you.
		.getxattr = fs_mac_getxattr,
#else
		.getxattr = fs_getxattr,
#endif
};

static int
opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs) {
	(void)data;

	switch (key) {
	case FUSE_OPT_KEY_NONOPT:
		if (options.archive == NULL) {
			options.archive = arg;
			return 0;
		}
		break;
	case KEY_HELP:
		fs_common_usage(outargs->argv[0]);
		fs_common_help();
		fuse_opt_add_arg(outargs, "-ho");
		fuse_main(outargs->argc, outargs->argv, &fs_oper, NULL);
		exit(0);
	case KEY_VERSION:
		fs_common_version("sqshfs2");
		fuse_opt_add_arg(outargs, "-V");
		fuse_main(outargs->argc, outargs->argv, &fs_oper, NULL);
		exit(0);
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

	return fuse_parse_cmdline(
			args, &options.mountpoint, &options.multithreaded,
			&options.foreground);
	if (rv < 0) {
		return rv;
	}

	if (options.archive == NULL) {
		fs_common_usage(args->argv[0]);
		return -1;
	}

	return 0;
}

static int
init_context(struct fuse_args *args, struct fuse **fuse) {
	int rv = 0;

	////////////////////////////////////////
	// Init archive
	context.archive = open_archive(options.archive, options.offset, &rv);
	if (rv < 0) {
		sqsh_perror(rv, options.archive);
		return rv;
	}

	////////////////////////////////////////
	// Init fuse
	context.fuse_ch = fuse_mount(options.mountpoint, args);
	if (!context.fuse_ch) {
		return -1;
	}

	*fuse = fuse_new(context.fuse_ch, args, &fs_oper, sizeof(fs_oper), NULL);
	if (*fuse == NULL) {
		return -1;
	}

	rv = fuse_daemonize(options.foreground);
	if (rv < 0) {
		return -1;
	}

	rv = fuse_set_signal_handlers(fuse_get_session(*fuse));
	if (rv < 0) {
		return rv;
	}

	return 0;
}

static void
cleanup_fuse(struct fuse *fuse) {
	if (fuse) {
		fuse_remove_signal_handlers(fuse_get_session(fuse));
	}
	if (context.fuse_ch) {
		fuse_unmount(options.mountpoint, context.fuse_ch);
	}
	if (fuse) {
		fuse_destroy(fuse);
	}
}

int
main(int argc, char *argv[]) {
	int rv;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse *fuse = NULL;

	rv = parse_args(&args);
	if (rv < 0) {
		goto out;
	}

	rv = init_context(&args, &fuse);
	if (rv < 0) {
		goto out;
	}

	if (options.multithreaded == 0) {
		rv = fuse_loop(fuse);
	} else {
		rv = fuse_loop_mt(fuse);
	}

out:
	sqsh_archive_close(context.archive);
	cleanup_fuse(fuse);

	fuse_opt_free_args(&args);
	free(options.mountpoint);
	return rv < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
