#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
	uint8_t *buf;
	size_t size;
	size_t used;
} od_linear_alloc_t;

static inline void od_linear_alloc_init(od_linear_alloc_t *al, uint8_t *buf,
					size_t size)
{
	al->buf = buf;
	al->size = size;
	al->used = 0;
}

static inline void od_linear_alloc_destroy(od_linear_alloc_t *al)
{
	(void)al;
}

static inline void od_linear_alloc_reset(od_linear_alloc_t *al)
{
	al->used = 0;
}

static inline void *od_linear_alloc_alloc(od_linear_alloc_t *al, size_t size)
{
	size_t align = sizeof(void *);
	size_t aligned = (size + align - 1) & ~(align - 1);

	if (al->used + aligned > al->size) {
		return NULL;
	}

	void *ptr = al->buf + al->used;
	al->used += aligned;
	memset(ptr, 0, aligned);

	return ptr;
}

static inline void od_linear_alloc_free(od_linear_alloc_t *al, void *ptr)
{
	(void)al;
	(void)ptr;
}
