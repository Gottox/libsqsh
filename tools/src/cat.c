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

#include <sqshtools_common.h>

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if defined(__OPENBSD__)
static void
lockdown(void) {
	pledge("stdio inet dns", NULL);
}
#elif defined(CONFIG_LANDLOCK)
static void
lockdown(void) {
	int fd;
	static const struct landlock_ruleset_attr rules = {
			.handled_access_fs = LANDLOCK_ACCESS_FS_READ_FILE,
	};

	fd = syscall(SYS_landlock_create_ruleset, &rules, sizeof(rules), 0);
	if (fd < 0) {
		perror("Failed to create a ruleset");
		goto err;
	}
	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
		perror("Failed to restrict privileges");
		goto err;
	}
	if (syscall(SYS_landlock_restrict_self, fd, 0)) {
		perror("Failed to enforce ruleset");
		goto err;
	}
	return;

err:
	if (fd >= 0) {
		close(fd);
	}
	exit(EXIT_FAILURE);
}
#else
#	define lockdown()
#endif

static int
usage(char *arg0) {
	printf("usage: %s [-o OFFSET] FILESYSTEM PATH [PATH ...]\n", arg0);
	printf("       %s -v\n", arg0);
	return EXIT_FAILURE;
}

static int
cat_path(struct SqshArchive *archive, char *path) {
	struct SqshFile *file = NULL;

	int rv = 0;
	file = sqsh_open(archive, path, &rv);
	if (rv < 0) {
		sqsh_perror(rv, path);
		rv = EXIT_FAILURE;
		goto out;
	}

	rv = sqsh_file_to_stream(file, stdout);
	if (rv < 0) {
		sqsh_perror(rv, path);
		rv = EXIT_FAILURE;
		goto out;
	}

out:
	sqsh_close(file);
	return rv;
}

static const char opts[] = "o:vh";
static const struct option long_opts[] = {
		{"offset", required_argument, NULL, 'o'},
		{"version", no_argument, NULL, 'v'},
		{"help", no_argument, NULL, 'h'},
		{0},
};

int
main(int argc, char *argv[]) {
	int rv = 0;
	int opt = 0;
	const char *image_path;
	struct SqshArchive *sqsh = NULL;
	uint64_t offset = 0;

	lockdown();

	while ((opt = getopt_long(argc, argv, opts, long_opts, NULL)) != -1) {
		switch (opt) {
		case 'o':
			offset = strtoull(optarg, NULL, 0);
			break;
		case 'v':
			puts("sqsh-cat-" VERSION);
			return 0;
		default:
			return usage(argv[0]);
		}
	}

	if (optind + 1 >= argc) {
		return usage(argv[0]);
	}

	image_path = argv[optind];
	optind++;

	sqsh = open_archive(image_path, offset, &rv);
	if (rv < 0) {
		sqsh_perror(rv, image_path);
		rv = EXIT_FAILURE;
		goto out;
	}

	for (; optind < argc; optind++) {
		rv = cat_path(sqsh, argv[optind]);
		if (rv < 0) {
			goto out;
		}
	}

out:
	sqsh_archive_close(sqsh);
	return rv;
}
