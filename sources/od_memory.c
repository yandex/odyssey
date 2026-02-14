#include <odyssey.h>

#include <machinarium/machinarium.h>
#include <machinarium/memory.h>

void *od_malloc(size_t size)
{
	return mm_malloc(size);
}

void od_free(void *ptr)
{
	mm_free(ptr);
}

void *od_calloc(size_t nmemb, size_t size)
{
	return mm_calloc(nmemb, size);
}

void *od_realloc(void *ptr, size_t size)
{
	return mm_realloc(ptr, size);
}

char *od_strdup(const char *s)
{
	return mm_strdup(s);
}

char *od_strndup(const char *s, size_t n)
{
	return mm_strndup(s, n);
}
