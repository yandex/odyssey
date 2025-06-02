#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

typedef void (*mm_context_function_t)(void *);

typedef struct mm_context mm_context_t;

struct mm_context {
	void **sp;
};

void mm_context_create(mm_context_t *, mm_contextstack_t *,
		       mm_context_function_t, void *);
void mm_context_swap(mm_context_t *, mm_context_t *);
