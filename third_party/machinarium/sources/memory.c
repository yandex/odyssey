#include <machinarium.h>
#include <machinarium_private.h>

/* TODO: add jealloc support and memory contexts here */

void *mm_malloc(size_t size)
{
	return malloc(size);
}

void mm_free(void *ptr)
{
	free(ptr);
}

void *mm_calloc(size_t nmemb, size_t size)
{
	return calloc(nmemb, size);
}

void *mm_realloc(void *ptr, size_t size)
{
	return realloc(ptr, size);
}

MACHINE_API void *machine_malloc(size_t size)
{
	return mm_malloc(size);
}

MACHINE_API void machine_free(void *ptr)
{
	mm_free(ptr);
}

MACHINE_API void *machine_calloc(size_t nmemb, size_t size)
{
	return mm_calloc(nmemb, size);
}

MACHINE_API void *machine_realloc(void *ptr, size_t size)
{
	return mm_realloc(ptr, size);
}