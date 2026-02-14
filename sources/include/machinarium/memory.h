#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <stddef.h>

void *mm_malloc(size_t size);
void mm_free(void *ptr);
void *mm_calloc(size_t nmemb, size_t size);
void *mm_realloc(void *ptr, size_t size);

char *mm_strdup(const char *s);
char *mm_strndup(const char *s, size_t n);
