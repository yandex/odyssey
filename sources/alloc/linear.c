#include <odyssey.h>

#include <alloc/linear.h>

typedef struct {
	size_t size;
} alloc_hdr_t;

static_assert(sizeof(alloc_hdr_t) <= alignof(max_align_t),
	      "invalid header definition");

#define UNWRAP_HDR(p) ((alloc_hdr_t *)((uint8_t *)p - alignof(max_align_t)))

void od_linear_alloc_init(od_linear_alloc_t *al, uint8_t *buf, size_t size)
{
	od_release_assert(((uintptr_t)buf & (alignof(max_align_t) - 1)) == 0);

	al->buf = buf;
	al->size = size;
	al->used = 0;
	al->track_sizes = 0;
}

void od_linear_alloc_reset(od_linear_alloc_t *al, int flags)
{
	al->used = 0;
	al->track_sizes = (flags & OD_LINEAR_ALLOC_REALLOC) != 0;
}

void *od_linear_alloc_alloc(od_linear_alloc_t *al, size_t size, int flags)
{
	/* compatibility with libc's malloc */
	if (size == 0) {
		size = 1;
	}

	size_t align = alignof(max_align_t);
	size_t aligned = (size + align - 1) & ~(align - 1);
	size_t hdr = al->track_sizes ? align : 0;
	size_t total = hdr + aligned;

	if (al->used + total > al->size) {
		return NULL;
	}

	uint8_t *tail = al->buf + al->used;
	void *ptr = tail + hdr;
	al->used += total;

	if (al->track_sizes) {
		((alloc_hdr_t *)tail)->size = aligned;
	}

	if (flags & OD_LINEAR_ALLOC_ZERO) {
		memset(ptr, 0, aligned);
	}

	return ptr;
}

void *od_linear_alloc_realloc(od_linear_alloc_t *al, void *ptr, size_t new_size)
{
	if (ptr == NULL) {
		return od_linear_alloc_alloc(al, new_size, 0);
	}

	if (!al->track_sizes) {
		/*
		 * without per-block headers the size of an old block is
		 * unknown, realloc of an existing block is supported only
		 * in OD_LINEAR_ALLOC_REALLOC mode
		 */
		return NULL;
	}

	if (new_size == 0) {
		od_linear_alloc_free(al, ptr);
		return NULL;
	}

	alloc_hdr_t *hdr = UNWRAP_HDR(ptr);
	size_t old_size = hdr->size;
	size_t align = alignof(max_align_t);
	new_size = (new_size + align - 1) & ~(align - 1);

	if (new_size == old_size) {
		return ptr;
	}

	/* maybe this was the last allocation and we can expand/shrink it? */
	if ((al->buf + al->used) == (((uint8_t *)ptr) + old_size)) {
		if (old_size < new_size) {
			size_t diff = new_size - old_size;
			if (al->used + diff > al->size) {
				return NULL;
			}

			al->used += diff;
		} else {
			al->used -= (old_size - new_size);
		}

		hdr->size = new_size;

		return ptr;
	}

	void *new_ptr = od_linear_alloc_alloc(al, new_size, 0);
	if (new_ptr == NULL) {
		return NULL;
	}

	if (old_size < new_size) {
		memcpy(new_ptr, ptr, old_size);
	} else {
		memcpy(new_ptr, ptr, new_size);
	}

	od_linear_alloc_free(al, ptr);

	return new_ptr;
}

void *od_linear_alloc_calloc(od_linear_alloc_t *al, size_t nmemb, size_t size)
{
	if (nmemb != 0 && size > SIZE_MAX / nmemb) {
		return NULL;
	}

	return od_linear_alloc_alloc(al, nmemb * size, OD_LINEAR_ALLOC_ZERO);
}

char *od_linear_alloc_strdup(od_linear_alloc_t *al, const char *s)
{
	size_t len = strlen(s) + 1;
	void *new = od_linear_alloc_alloc(al, len, 0);

	if (new == NULL) {
		return NULL;
	}

	return (char *)memcpy(new, s, len);
}
char *od_linear_alloc_strndup(od_linear_alloc_t *al, const char *s, size_t n)
{
	size_t len = strnlen(s, n);
	char *new = (char *)od_linear_alloc_alloc(al, len + 1, 0);

	if (new == NULL) {
		return NULL;
	}

	new[len] = '\0';
	return (char *)memcpy(new, s, len);
}
