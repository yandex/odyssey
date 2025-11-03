#include <machinarium.h>
#include <machinarium_private.h>

/* TODO: add jealloc support and memory contexts here */

static inline void *wrap_allocation(void *d)
{
	if (mm_likely(d != NULL)) {
		return d;
	}

	if (mm_self != NULL) {
		mm_errno_set(ENOMEM);
	}

	return NULL;
}

void *mm_malloc(size_t size)
{
	void *mem = malloc(size);

	return wrap_allocation(mem);
}

void mm_free(void *ptr)
{
	free(ptr);
}

void *mm_calloc(size_t nmemb, size_t size)
{
	void *mem = calloc(nmemb, size);

	return wrap_allocation(mem);
}

void *mm_realloc(void *ptr, size_t size)
{
	void *mem = realloc(ptr, size);

	return wrap_allocation(mem);
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
