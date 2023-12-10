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
 * @file         tree_traversal.c
 */

#define _DEFAULT_SOURCE

#include "sqsh_directory.h"
#include <sqsh_tree_private.h>

#include <sqsh_archive_private.h>
#include <sqsh_common_private.h>
#include <sqsh_directory_private.h>
#include <sqsh_error.h>
#include <sqsh_file_private.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define STACK_BUCKET_SIZE 16

#define STACK_GET(t, i) \
	(&t->stack[(i) / STACK_BUCKET_SIZE][(i) % STACK_BUCKET_SIZE])

#define STACK_PEEK(t) STACK_GET(t, (t)->stack_size - 1)

#define STACK_PEEK_ITERATOR(t) \
	((t)->stack_size ? &STACK_PEEK(t)->iterator : &(t)->base_iterator)

static int
ensure_stack_capacity(
		struct SqshTreeTraversal *traversal, size_t new_capacity) {
	int rv = 0;
	if (new_capacity <= traversal->stack_capacity) {
		return 0;
	}

	size_t outer_capacity = SQSH_DIVIDE_CEIL(new_capacity, STACK_BUCKET_SIZE);
	new_capacity = outer_capacity * STACK_BUCKET_SIZE;

	struct SqshTreeTraversalStackElement **new_stack = NULL;
	new_stack = reallocarray(
			traversal->stack, outer_capacity,
			sizeof(struct SqshTreeTraversalStackElement *));
	if (new_stack == NULL) {
		rv = -SQSH_ERROR_MALLOC_FAILED;
		goto out;
	}

	struct SqshTreeTraversalStackElement *new_bucket = calloc(
			STACK_BUCKET_SIZE, sizeof(struct SqshTreeTraversalStackElement));
	if (new_bucket == NULL) {
		rv = -SQSH_ERROR_MALLOC_FAILED;
		goto out;
	}
	new_stack[outer_capacity - 1] = new_bucket;

	traversal->stack = new_stack;
	traversal->stack_capacity = new_capacity;
out:
	return rv;
}

static int
stack_add(struct SqshTreeTraversal *traversal, const uint64_t inode_ref) {
	int rv;
	const size_t new_size = traversal->stack_size + 1;
	rv = ensure_stack_capacity(traversal, new_size);
	if (rv < 0) {
		goto out;
	}
	traversal->stack_size = new_size;

	struct SqshTreeTraversalStackElement *element = STACK_PEEK(traversal);

	rv = sqsh__file_init(
			&element->file, traversal->base_file->archive, inode_ref);
	if (rv < 0) {
		goto out;
	}

	rv = sqsh__directory_iterator_init(&element->iterator, &element->file);
	if (rv < 0) {
		goto out;
	}

out:
	return rv;
}

static int
stack_pop(struct SqshTreeTraversal *traversal) {
	int rv;
	if (traversal->stack_size == 0) {
		return 0;
	}
	struct SqshTreeTraversalStackElement *element = STACK_PEEK(traversal);

	rv = sqsh__directory_iterator_cleanup(&element->iterator);
	if (rv < 0) {
		goto out;
	}

	rv = sqsh__file_cleanup(&element->file);
	if (rv < 0) {
		goto out;
	}

	traversal->stack_size--;
out:
	return rv;
}

int
sqsh__tree_traversal_init(
		struct SqshTreeTraversal *traversal, const struct SqshFile *file) {
	int rv;
	traversal->base_file = file;
	rv = sqsh__directory_iterator_init(&traversal->base_iterator, file);
	if (rv < 0) {
		goto out;
	}

out:
	if (rv < 0) {
		sqsh__directory_iterator_cleanup(&traversal->base_iterator);
	}
	return rv;
}

struct SqshTreeTraversal *
sqsh_tree_traversal_new(const struct SqshFile *file, int *err) {
	SQSH_NEW_IMPL(sqsh__tree_traversal_init, struct SqshTreeTraversal, file);
}

bool
sqsh_tree_traversal_next(struct SqshTreeTraversal *traversal, int *err) {
	bool has_next = false;
	int rv = 0;

	struct SqshDirectoryIterator *iterator = STACK_PEEK_ITERATOR(traversal);

	if (sqsh_tree_traversal_state(traversal) ==
		SQSH_TREE_TRAVERSAL_STATE_DIRECTORY_BEGIN) {
		rv = stack_add(traversal, sqsh_directory_iterator_inode_ref(iterator));
		if (rv < 0) {
			goto out;
		}
		iterator = STACK_PEEK_ITERATOR(traversal);
	}
	if (sqsh_directory_iterator_next(iterator, err)) {
		has_next = true;
		if (sqsh_directory_iterator_file_type(iterator) ==
			SQSH_FILE_TYPE_DIRECTORY) {
			traversal->state = SQSH_TREE_TRAVERSAL_STATE_DIRECTORY_BEGIN;
		} else {
			traversal->state = SQSH_TREE_TRAVERSAL_STATE_FILE;
		}
	} else {
		traversal->state = SQSH_TREE_TRAVERSAL_STATE_DIRECTORY_END;
		if (traversal->stack_size > 0) {
			rv = stack_pop(traversal);
			if (rv < 0) {
				goto out;
			}
			has_next = true;
		}
	}

out:
	if (err != NULL) {
		*err = rv;
	}
	return has_next;
}

enum SqshFileType
sqsh_tree_traversal_type(const struct SqshTreeTraversal *traversal) {
	return sqsh_directory_iterator_file_type(STACK_PEEK_ITERATOR(traversal));
}

enum SqshTreeTraversalState
sqsh_tree_traversal_state(const struct SqshTreeTraversal *traversal) {
	return traversal->state;
}

const char *
sqsh_tree_traversal_name(const struct SqshTreeTraversal *traversal) {
	return sqsh_directory_iterator_name(STACK_PEEK_ITERATOR(traversal));
}

static const struct SqshDirectoryIterator *
get_iterator(const struct SqshTreeTraversal *traversal, sqsh_index_t index) {
	if (index == 0) {
		return &traversal->base_iterator;
	}
	return &STACK_GET(traversal, index - 1)->iterator;
}

const char *
sqsh_tree_traversal_path_segment(
		const struct SqshTreeTraversal *traversal, sqsh_index_t index) {
	const struct SqshDirectoryIterator *iterator =
			get_iterator(traversal, index);
	if (iterator == NULL) {
		return NULL;
	}
	return sqsh_directory_iterator_name(iterator);
}

size_t
sqsh_tree_traversal_path_segment_size(
		const struct SqshTreeTraversal *traversal, sqsh_index_t index) {
	const struct SqshDirectoryIterator *iterator =
			get_iterator(traversal, index);
	if (iterator == NULL) {
		return 0;
	}
	return sqsh_directory_iterator_name_size(iterator);
}

size_t
sqsh_tree_traversal_depth(const struct SqshTreeTraversal *traversal) {
	return traversal->stack_size;
}

char *
sqsh_tree_traversal_path_dup(const struct SqshTreeTraversal *traversal) {
	struct CxBuffer buffer = {0};
	int rv = 0;

	rv = cx_buffer_init(&buffer);
	if (rv < 0) {
		goto out;
	}

	const size_t count = sqsh_tree_traversal_depth(traversal);
	for (sqsh_index_t i = 0; i < count; i++) {
		rv = cx_buffer_append(&buffer, (const uint8_t *)"/", 1);
		if (rv < 0) {
			goto out;
		}
		const char *name = sqsh_tree_traversal_path_segment(traversal, i);
		size_t size = sqsh_tree_traversal_path_segment_size(traversal, i);
		rv = cx_buffer_append(&buffer, (const uint8_t *)name, size);
		if (rv < 0) {
			goto out;
		}
	}

	// append null byte
	rv = cx_buffer_append(&buffer, (const uint8_t *)"", 1);
out:
	if (rv < 0) {
		cx_buffer_cleanup(&buffer);
		return NULL;
	} else {
		return (char *)cx_buffer_unwrap(&buffer);
	}
}

uint16_t
sqsh_tree_traversal_name_size(const struct SqshTreeTraversal *traversal) {
	return sqsh_directory_iterator_name_size(STACK_PEEK_ITERATOR(traversal));
}

char *
sqsh_tree_traversal_name_dup(const struct SqshTreeTraversal *traversal) {
	return sqsh_directory_iterator_name_dup(STACK_PEEK_ITERATOR(traversal));
}

struct SqshFile *
sqsh_tree_traversal_open_file(
		const struct SqshTreeTraversal *traversal, int *err) {
	return sqsh_directory_iterator_open_file(
			STACK_PEEK_ITERATOR(traversal), err);
}

int
sqsh__tree_traversal_cleanup(struct SqshTreeTraversal *traversal) {
	for (sqsh_index_t i = 0; i < traversal->stack_size; i++) {
		struct SqshTreeTraversalStackElement *element = STACK_GET(traversal, i);
		sqsh__directory_iterator_cleanup(&element->iterator);
		sqsh__file_cleanup(&element->file);
	}
	size_t outer_capacity = traversal->stack_capacity / STACK_BUCKET_SIZE;
	for (sqsh_index_t i = 0; i < outer_capacity; i++) {
		free(traversal->stack[i]);
	}
	free(traversal->stack);
	sqsh__directory_iterator_cleanup(&traversal->base_iterator);
	memset(traversal, 0, sizeof(*traversal));
	return 0;
}

const struct SqshDirectoryIterator *
sqsh_tree_traversal_iterator(const struct SqshTreeTraversal *traversal) {
	return STACK_PEEK_ITERATOR(traversal);
}

int
sqsh_tree_traversal_free(struct SqshTreeTraversal *traversal) {
	SQSH_FREE_IMPL(sqsh__tree_traversal_cleanup, traversal);
}
