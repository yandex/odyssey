#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <stddef.h>

#include <machinarium/build.h>

void *mm_malloc(size_t size);
void mm_free(void *ptr);
void *mm_calloc(size_t nmemb, size_t size);
void *mm_realloc(void *ptr, size_t size);

char *mm_strdup(const char *s);
char *mm_strndup(const char *s, size_t n);

#ifdef MM_MEM_PROF
static inline void *mm_malloc_crypto(size_t size, const char *file, int line)
{
	(void)file;
	(void)line;
	return mm_malloc(size);
}

static inline void mm_free_crypto(void *ptr, const char *file, int line)
{
	(void)file;
	(void)line;
	mm_free(ptr);
}

static inline void *mm_realloc_crypto(void *ptr, size_t size, const char *file,
				      int line)
{
	(void)file;
	(void)line;
	return mm_realloc(ptr, size);
}
#endif
