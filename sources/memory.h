#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stddef.h>

void *od_malloc(size_t size);
void od_free(void *ptr);
void *od_calloc(size_t nmemb, size_t size);
void *od_realloc(void *ptr, size_t size);
