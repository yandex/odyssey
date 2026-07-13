#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 *
 * yet another heap implementation
 * this one is NOT thread-safe and is highly inspired by libuv implementation
 */

#include <stddef.h>

typedef struct mm_heap_node {
	struct mm_heap_node *left;
	struct mm_heap_node *right;
	struct mm_heap_node *parent;
} mm_heap_node_t;

/* non-zero if a < b */
typedef int (*mm_heap_cmp_fn)(const mm_heap_node_t *a, const mm_heap_node_t *b);

typedef struct {
	mm_heap_node_t *min;
	mm_heap_cmp_fn cmp;
	size_t size;
} mm_heap_t;

void mm_heap_init(mm_heap_t *h, mm_heap_cmp_fn cmp);
mm_heap_node_t *mm_heap_min(mm_heap_t *h);
size_t mm_heap_size(mm_heap_t *h);
void mm_heap_push(mm_heap_t *h, mm_heap_node_t *node);
void mm_heap_remove(mm_heap_t *h, mm_heap_node_t *node);
mm_heap_node_t *mm_heap_pop(mm_heap_t *h);
int mm_heap_validate(mm_heap_t *h);
