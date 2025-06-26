#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium/context_stack.h>

typedef void (*mm_context_function_t)(void *);

typedef struct mm_context mm_context_t;

struct mm_context {
	void **sp;
#ifdef HAVE_TSAN
	void *tsan_fiber;
	int destroying;
	void *exit_from;
#endif
};

void mm_context_create(mm_context_t *, mm_contextstack_t *,
		       mm_context_function_t, void *);

void mm_context_swap_impl(mm_context_t *, mm_context_t *);

#ifdef HAVE_TSAN
void mm_context_swap(mm_context_t *current, mm_context_t *new);
#else
static inline void mm_context_swap(mm_context_t *current, mm_context_t *new)
{
	mm_context_swap_impl(current, new);
}
#endif
