#include <odyssey.h>

#include <machinarium/machinarium.h>

void *od_malloc(size_t size)
{
	return machine_malloc(size);
}

void od_free(void *ptr)
{
	machine_free(ptr);
}

void *od_calloc(size_t nmemb, size_t size)
{
	return machine_calloc(nmemb, size);
}

void *od_realloc(void *ptr, size_t size)
{
	return machine_realloc(ptr, size);
}
