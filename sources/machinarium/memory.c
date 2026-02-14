#include <errno.h>

#include <machinarium/machinarium.h>
#include <machinarium/machine.h>
#include <machinarium/coroutine.h>

#ifndef MM_MEM_PROF
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

#else
typedef struct {
	uint64_t size;
} mm_alloc_header_t;

static inline void *wrap_allocation(void *d)
{
	if (mm_likely(d != NULL)) {
		return ((mm_alloc_header_t *)d) + 1;
	}

	if (mm_self != NULL) {
		mm_errno_set(ENOMEM);
	}

	return NULL;
}

static inline void *unwrap_profiled_ptr(void *d)
{
	if (d != NULL) {
		return ((mm_alloc_header_t *)d) - 1;
	}

	return NULL;
}

static inline void *malloc_internal(size_t size, int set_zero)
{
	void *mem = malloc(sizeof(mm_alloc_header_t) + size);
	if (mem != NULL) {
		mm_alloc_header_t *hdr = mem;
		hdr->size = size;

		if (set_zero) {
			memset(hdr + 1, 0, size);
		}

		if (mm_self != NULL) {
			mm_coroutine_t *coro = mm_self->scheduler.current;

			if (coro != NULL) {
				coro->allocated_bytes += size;
			} else {
				mm_self->allocated_bytes += size;
			}
		}
	}

	return wrap_allocation(mem);
}

void *mm_malloc(size_t size)
{
	return malloc_internal(size, 0 /* set_zero */);
}

void mm_free(void *ptr)
{
	mm_alloc_header_t *hdr = unwrap_profiled_ptr(ptr);
	if (hdr != NULL) {
		if (mm_self != NULL) {
			mm_coroutine_t *coro = mm_self->scheduler.current;

			if (coro != NULL) {
				coro->freed_bytes += hdr->size;
			} else {
				mm_self->freed_bytes += hdr->size;
			}
		}

		free(hdr);
	}
}

void *mm_realloc(void *ptr, size_t size)
{
	if (ptr == NULL) {
		return malloc_internal(size, 0 /* set_zero */);
	}

	if (size == 0) {
		mm_free(ptr);
		return NULL;
	}

	mm_alloc_header_t *hdr = unwrap_profiled_ptr(ptr);
	uint64_t old_size = hdr->size;

	mm_alloc_header_t *new_hdr =
		realloc(hdr, sizeof(mm_alloc_header_t) + size);
	if (new_hdr != NULL) {
		new_hdr->size = size;

		if (mm_self != NULL) {
			mm_coroutine_t *coro = mm_self->scheduler.current;

			if (coro != NULL) {
				coro->freed_bytes += old_size;
				coro->allocated_bytes += size;
			} else {
				mm_self->freed_bytes += old_size;
				mm_self->allocated_bytes += size;
			}
		}
	} else {
		/* nothing should be accounted */
	}

	return wrap_allocation(new_hdr);
}

void *mm_calloc(size_t nmemb, size_t size)
{
	if (nmemb != 0 && size > SIZE_MAX / nmemb) {
		if (mm_self != NULL) {
			mm_errno_set(ENOMEM);
		}
		return NULL;
	}

	return malloc_internal(nmemb * size, 1 /* set_zero */);
}
#endif /* ifndef MM_MEM_PROF */

char *mm_strdup(const char *s)
{
	size_t len = strlen(s) + 1;
	void *new = mm_malloc(len);

	if (new == NULL) {
		return NULL;
	}

	return (char *)memcpy(new, s, len);
}

char *mm_strndup(const char *s, size_t n)
{
	size_t len = strnlen(s, n);
	char *new = (char *)mm_malloc(len + 1);

	if (new == NULL) {
		return NULL;
	}

	new[len] = '\0';
	return (char *)memcpy(new, s, len);
}
