#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

/*
 * yet another impl for dynamic size array
 */

#include <stdint.h>
#include <stddef.h>

typedef void (*mm_vector_element_dtor_fn)(void *element);

typedef struct {
	uint8_t *elements;
	mm_vector_element_dtor_fn eldtor;
	size_t elsize;
	size_t size;
	size_t capacity;
} mm_vector_t;

void mm_vector_init(mm_vector_t *vec, size_t elsize,
		    mm_vector_element_dtor_fn eldtor);
void mm_vector_destroy(mm_vector_t *vec);
size_t mm_vector_size(const mm_vector_t *vec);
void mm_vector_clear(mm_vector_t *vec);
void *mm_vector_get(mm_vector_t *vec, size_t idx);
void *mm_vector_back(mm_vector_t *vec);
int mm_vector_append(mm_vector_t *vec, const void *val);
