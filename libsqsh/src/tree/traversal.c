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
 * @file         traversal.c
 */

#include <sqsh_tree_private.h>

#include <sqsh_archive_private.h>
#include <sqsh_common_private.h>

#define RECURSION_CHECK_DEPTH 128

#define STACK_BUCKET_SIZE 16

#define STACK_GET(t, i) \
	(&t->stack[(i) / STACK_BUCKET_SIZE][(i) % STACK_BUCKET_SIZE])

#define STACK_PEEK(t) STACK_GET(t, (t)->stack_size - 1)

static int
check_recursion(const struct SqshTreeTraversal *traversal, uint64_t inode_ref) {
	const struct SqshDirectoryIterator *iterator;

	const size_t count = sqsh_tree_traversal_depth(traversal);
	// This function is only called when the stack size is greater than
	// RECURSION_CHECK_DEPTH, so we can safely ignore underflows of count.
	for (sqsh_index_t i = 0; i < count - 1; i++) {
		iterator = &STACK_GET(traversal, i)->iterator;
		if (sqsh_directory_iterator_inode_ref(iterator) == inode_ref) {
			return -SQSH_ERROR_DIRECTORY_RECURSION;
		}
	}
	return 0;
}

static int
ensure_stack_capacity(
		struct SqshTreeTraversal *traversal, size_t new_capacity) {
	int rv = 0;
	struct SqshTreeTraversalStackElement **new_stack = NULL;
	struct SqshTreeTraversalStackElement *new_bucket = NULL;
	if (new_capacity <= traversal->stack_capacity) {
		return 0;
	}

	size_t outer_capacity = SQSH_DIVIDE_CEIL(new_capacity, STACK_BUCKET_SIZE);
	new_capacity = outer_capacity * STACK_BUCKET_SIZE;

	size_t size;
	if (SQSH_MULT_OVERFLOW(outer_capacity, sizeof(*new_stack), &size)) {
		rv = -SQSH_ERROR_INTEGER_OVERFLOW;
		goto out;
	}
	new_stack = realloc(traversal->stack, size);
	if (new_stack == NULL) {
		rv = -SQSH_ERROR_MALLOC_FAILED;
		goto out;
	}

	new_bucket = calloc(
			STACK_BUCKET_SIZE, sizeof(struct SqshTreeTraversalStackElement));
	if (new_bucket == NULL) {
		rv = -SQSH_ERROR_MALLOC_FAILED;
		goto out;
	}
	new_stack[outer_capacity - 1] = new_bucket;
	new_bucket = NULL;

	traversal->stack = new_stack;
	traversal->stack_capacity = new_capacity;
	new_stack = NULL;
out:
	free(new_stack);
	free(new_bucket);
	return rv;
}

static int
stack_add(struct SqshTreeTraversal *traversal) {
	const size_t new_size = traversal->stack_size + 1;
	int rv = ensure_stack_capacity(traversal, new_size);
	uint64_t inode_ref = traversal->current_inode_ref;
	struct SqshTreeTraversalStackElement *element = NULL;
	if (rv < 0) {
		goto out;
	}
	traversal->stack_size = new_size;

	element = STACK_PEEK(traversal);

	rv = sqsh__file_init(
			&element->file, traversal->base_file->archive, inode_ref);
	if (rv < 0) {
		goto out;
	}

	rv = sqsh__directory_iterator_init(&element->iterator, &element->file);
	if (rv < 0) {
		goto out;
	}

	if (traversal->stack_size >= RECURSION_CHECK_DEPTH) {
		rv = check_recursion(traversal, inode_ref);
	}

	traversal->current_iterator = &element->iterator;
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

	traversal->current_iterator = &STACK_PEEK(traversal)->iterator;

out:
	return rv;
}

int
sqsh__tree_traversal_init(
		struct SqshTreeTraversal *traversal, size_t max_depth,
		const struct SqshFile *file) {
	int rv = 0;

	traversal->state = SQSH_TREE_TRAVERSAL_STATE_INIT;
	traversal->base_file = file;
	traversal->stack = NULL;
	traversal->stack_size = 0;
	traversal->stack_capacity = 0;
	traversal->max_depth = max_depth ? max_depth : SIZE_MAX;

	return rv;
}

struct SqshTreeTraversal *
sqsh_tree_traversal_new(const struct SqshFile *file, int *err) {
	return sqsh_tree_traversal_new2(file, 0, err);
}

struct SqshTreeTraversal *
sqsh_tree_traversal_new2(
		const struct SqshFile *file, size_t max_depth, int *err) {
	SQSH_NEW_IMPL(
			sqsh__tree_traversal_init, struct SqshTreeTraversal, max_depth,
			file);
}

static void
update_state_from_type(struct SqshTreeTraversal *traversal) {
	if (traversal->max_depth <= traversal->stack_size) {
		traversal->state = SQSH_TREE_TRAVERSAL_STATE_FILE;
	} else if (traversal->current_type == SQSH_FILE_TYPE_DIRECTORY) {
		traversal->state = SQSH_TREE_TRAVERSAL_STATE_DIRECTORY_BEGIN;
	} else {
		traversal->state = SQSH_TREE_TRAVERSAL_STATE_FILE;
	}
}

static void
update_from_iterator(struct SqshTreeTraversal *traversal) {
	const struct SqshDirectoryIterator *iterator = traversal->current_iterator;
	size_t current_name_size = 0;
	traversal->current_name =
			sqsh_directory_iterator_name2(iterator, &current_name_size);
	traversal->current_name_size = current_name_size;
	traversal->current_inode_ref = sqsh_directory_iterator_inode_ref(iterator);
	traversal->current_type = sqsh_directory_iterator_file_type(iterator);
}

bool
sqsh_tree_traversal_next(struct SqshTreeTraversal *traversal, int *err) {
	bool has_next = false;
	int rv = 0;
	struct SqshDirectoryIterator *iterator = NULL;

	switch (sqsh_tree_traversal_state(traversal)) {
	case SQSH_TREE_TRAVERSAL_STATE_INIT:
		traversal->current_name = "";
		traversal->current_name_size = 0;
		traversal->current_inode_ref =
				sqsh_file_inode_ref(traversal->base_file);
		traversal->current_type = sqsh_file_type(traversal->base_file);
		update_state_from_type(traversal);
		has_next = true;
		goto out;
	case SQSH_TREE_TRAVERSAL_STATE_DIRECTORY_BEGIN:
		rv = stack_add(traversal);
		if (rv < 0) {
			goto out;
		}
		break;
	case SQSH_TREE_TRAVERSAL_STATE_FILE:
		if (traversal->current_iterator == NULL) {
			goto out;
		}
		break;
	default:
		break;
	}

	iterator = traversal->current_iterator;
	if (sqsh_directory_iterator_next(iterator, err)) {
		has_next = true;
		update_from_iterator(traversal);
		update_state_from_type(traversal);
	} else {
		traversal->state = SQSH_TREE_TRAVERSAL_STATE_DIRECTORY_END;
		if (traversal->stack_size > 1) {
			rv = stack_pop(traversal);
			if (rv < 0) {
				goto out;
			}
			update_from_iterator(traversal);
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
	return sqsh_directory_iterator_file_type(traversal->current_iterator);
}

enum SqshTreeTraversalState
sqsh_tree_traversal_state(const struct SqshTreeTraversal *traversal) {
	return traversal->state;
}

const char *
sqsh_tree_traversal_name(
		const struct SqshTreeTraversal *traversal, size_t *len) {
	*len = traversal->current_name_size;
	return traversal->current_name;
}

const char *
sqsh_tree_traversal_path_segment(
		const struct SqshTreeTraversal *traversal, size_t *len,
		sqsh_index_t index) {
	const struct SqshDirectoryIterator *iterator =
			&STACK_GET(traversal, index)->iterator;
	return sqsh_directory_iterator_name2(iterator, len);
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
		if (i != 0) {
			rv = cx_buffer_append(&buffer, (const uint8_t *)"/", 1);
			if (rv < 0) {
				goto out;
			}
		}

		size_t size = 0;
		const char *name =
				sqsh_tree_traversal_path_segment(traversal, &size, i);
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

char *
sqsh_tree_traversal_name_dup(const struct SqshTreeTraversal *traversal) {
	if (traversal->current_iterator != NULL) {
		return sqsh_directory_iterator_name_dup(traversal->current_iterator);
	} else {
		// return empty string for root
		return calloc(1, sizeof(char));
	}
}

struct SqshFile *
sqsh_tree_traversal_open_file(
		const struct SqshTreeTraversal *traversal, int *err) {
	if (traversal->current_iterator == NULL) {
		return sqsh_open_by_ref(
				traversal->base_file->archive, traversal->current_inode_ref,
				err);
	} else {
		return sqsh_directory_iterator_open_file(
				traversal->current_iterator, err);
	}
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
	memset(traversal, 0, sizeof(*traversal));
	return 0;
}

const struct SqshDirectoryIterator *
sqsh_tree_traversal_iterator(const struct SqshTreeTraversal *traversal) {
	return traversal->current_iterator;
}

int
sqsh_tree_traversal_free(struct SqshTreeTraversal *traversal) {
	SQSH_FREE_IMPL(sqsh__tree_traversal_cleanup, traversal);
}
