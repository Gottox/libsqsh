/**
 * @author      : Enno Boland (mail@eboland.de)
 * @file        : common.h
 */

#ifndef BIN_COMMON_H

#define BIN_COMMON_H

#include "../src/sqsh.h"
#include <ctype.h>
#include <string.h>

static struct Sqsh *
open_archive(const char *image_path, int *err) {
	struct SqshConfig config = {
			.source_type = SQSH_SOURCE_TYPE_PATH,
	};
#ifdef CONFIG_CURL
	int i;
	for (i = 0; isalnum(image_path[i]); i++) {
	}
	if (strncmp(&image_path[i], "://", 3) == 0) {
		config.source_type = SQSH_SOURCE_TYPE_CURL;
	}
#endif

	return sqsh_new(image_path, &config, err);
}

#endif /* end of include guard COMMON_H */
