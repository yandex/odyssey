#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <types.h>

void *od_malloc(size_t size);
void od_free(void *ptr);
void *od_calloc(size_t nmemb, size_t size);
void *od_realloc(void *ptr, size_t size);

char *od_strdup(const char *s);
char *od_strndup(const char *s, size_t n);

/* can be set to NULL to disable usage of alloc */
void od_set_thread_linear_alloc(od_linear_alloc_t *la);
void od_oom_fatal(void);
void od_set_thread_oom_hook(void (*hook)(void));
