#include <odyssey.h>

#include <machinarium/machinarium.h>
#include <machinarium/memory.h>

#include <alloc/linear.h>

static OD_THREAD_LOCAL od_linear_alloc_t *_current_linear_alloc = NULL;

void od_set_thread_linear_alloc(od_linear_alloc_t *la)
{
	_current_linear_alloc = la;
}

static od_linear_alloc_t *current_linear_alloc(void)
{
	return _current_linear_alloc;
}

void *od_malloc(size_t size)
{
	od_linear_alloc_t *la = current_linear_alloc();
	if (la == NULL) {
		return mm_malloc(size);
	}

	return od_linear_alloc_alloc(la, size, 0);
}

void od_free(void *ptr)
{
	od_linear_alloc_t *la = current_linear_alloc();
	if (la == NULL) {
		mm_free(ptr);
	} else {
		od_linear_alloc_free(la, ptr);
	}
}

void *od_calloc(size_t nmemb, size_t size)
{
	od_linear_alloc_t *la = current_linear_alloc();
	if (la == NULL) {
		return mm_calloc(nmemb, size);
	}

	return od_linear_alloc_calloc(la, nmemb, size);
}

void *od_realloc(void *ptr, size_t size)
{
	od_linear_alloc_t *la = current_linear_alloc();
	if (la == NULL) {
		return mm_realloc(ptr, size);
	}

	return od_linear_alloc_realloc(la, ptr, size);
}

char *od_strdup(const char *s)
{
	od_linear_alloc_t *la = current_linear_alloc();
	if (la == NULL) {
		return mm_strdup(s);
	}

	return od_linear_alloc_strdup(la, s);
}

char *od_strndup(const char *s, size_t n)
{
	od_linear_alloc_t *la = current_linear_alloc();
	if (la == NULL) {
		return mm_strndup(s, n);
	}

	return od_linear_alloc_strndup(la, s, n);
}
