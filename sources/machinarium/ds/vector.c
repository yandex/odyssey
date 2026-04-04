#include <string.h>
#include <stdlib.h>

#include <machinarium/memory.h>
#include <machinarium/ds/vector.h>

void mm_vector_init(mm_vector_t *vec, size_t elsize,
		    mm_vector_element_dtor_fn eldtor)
{
	memset(vec, 0, sizeof(mm_vector_t));

	if (elsize == 0) {
		abort();
	}

	vec->eldtor = eldtor;
	vec->elsize = elsize;
}

void mm_vector_destroy(mm_vector_t *vec)
{
	mm_vector_clear(vec);

	mm_free(vec->elements);

	vec->capacity = 0;
	vec->elements = NULL;
}

size_t mm_vector_size(const mm_vector_t *vec)
{
	return vec->size;
}

static inline void *get_impl(mm_vector_t *vec, size_t idx)
{
	return &vec->elements[idx * vec->elsize];
}

void mm_vector_clear(mm_vector_t *vec)
{
	if (vec->eldtor != NULL) {
		size_t sz = mm_vector_size(vec);

		for (size_t i = 0; i < sz; ++i) {
			vec->eldtor(get_impl(vec, i));
		}
	}

	vec->size = 0;
}

void *mm_vector_get(mm_vector_t *vec, size_t idx)
{
	if (idx < vec->size) {
		return get_impl(vec, idx);
	}

	abort();
}

void *mm_vector_back(mm_vector_t *vec)
{
	return mm_vector_get(vec, mm_vector_size(vec) - 1);
}

int mm_vector_append(mm_vector_t *vec, const void *val)
{
	if (vec->size >= vec->capacity) {
		size_t nc = vec->capacity == 0 ? 16 : vec->capacity * 2;
		void *ne = mm_realloc(vec->elements, nc * vec->elsize);
		if (ne == NULL) {
			return -1;
		}

		vec->elements = ne;
		vec->capacity = nc;
	}

	void *n = get_impl(vec, vec->size++);
	if (val != NULL) {
		memcpy(n, val, vec->elsize);
	} else {
		memset(n, 0, vec->elsize);
	}

	return 0;
}
