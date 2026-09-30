#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <types.h>

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef enum {
	OD_LINEAR_ALLOC_ZERO = 1 << 0,
	OD_LINEAR_ALLOC_REALLOC = 1 << 1,
} od_linear_alloc_flags_t;

struct od_linear_alloc {
	uint8_t *buf;
	size_t size;
	size_t used;
	int track_sizes;
};

static inline void od_linear_alloc_destroy(od_linear_alloc_t *al)
{
	(void)al;
}

static inline void od_linear_alloc_free(od_linear_alloc_t *al, void *ptr)
{
	(void)al;
	(void)ptr;
}

/*
 * must be called before every usage of the arena, it drops all previous
 * allocations and sets the arena mode
 */
void od_linear_alloc_init(od_linear_alloc_t *al, uint8_t *buf, size_t size);
void od_linear_alloc_reset(od_linear_alloc_t *al, int flags);
void *od_linear_alloc_alloc(od_linear_alloc_t *al, size_t size, int flags);
void *od_linear_alloc_calloc(od_linear_alloc_t *al, size_t nmemb, size_t size);
void *od_linear_alloc_realloc(od_linear_alloc_t *al, void *ptr,
			      size_t new_size);
char *od_linear_alloc_strdup(od_linear_alloc_t *al, const char *s);
char *od_linear_alloc_strndup(od_linear_alloc_t *al, const char *s, size_t n);
