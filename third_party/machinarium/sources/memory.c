#include <machinarium.h>
#include <machinarium_private.h>

/* TODO: add jealloc support and memory contexts here */

void *alloc_failed()
{
	mm_errno_set(ENOMEM);
	return NULL;
}

void *wrap_alloc(void *result)
{
	if (result != NULL) {
		return result;
	}

	return alloc_failed();
}

void *mm_malloc(size_t size)
{
	void *r = malloc(size);
	return wrap_alloc(r);
}

void mm_free(void *ptr)
{
	free(ptr);
}

void *mm_calloc(size_t nmemb, size_t size)
{
	void *r = calloc(nmemb, size);
	return wrap_alloc(r);
}

void *mm_realloc(void *ptr, size_t size)
{
	void *r = realloc(ptr, size);
	return wrap_alloc(r);
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
