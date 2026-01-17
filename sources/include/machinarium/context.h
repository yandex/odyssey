#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#ifdef USE_UCONTEXT
#include <ucontext.h>
#endif

#include <machinarium/context_stack.h>

typedef void (*mm_context_function_t)(void *);

typedef struct mm_context mm_context_t;

struct mm_context {
#ifdef USE_UCONTEXT
	ucontext_t uctx;
#else
	void **sp;
#endif

#ifdef HAVE_TSAN
	void *tsan_fiber;
	int destroying;
	mm_context_t *exit_from;
#endif
};

void mm_context_create(mm_context_t *, mm_contextstack_t *,
		       mm_context_function_t, void *);

/*
 * if the build is with TSAN, then the wrapper function must be used
 * otherwise, just make proxy inlined function to impl
 * 
 * mm_context_swap possibly adds TSAN wrapper and then calls
 * mm_context_swap_private, which select ucontext if must
 */

#ifndef USE_UCONTEXT
void mm_context_swap_impl(mm_context_t *, mm_context_t *);
#endif

static inline void mm_context_swap_private(mm_context_t *current,
					   mm_context_t *new)
{
#ifdef USE_UCONTEXT
	swapcontext(&current->uctx, &new->uctx);
#else
	mm_context_swap_impl(current, new);
#endif /* USE_UCONTEXT */
}

#ifdef HAVE_TSAN
void mm_context_swap(mm_context_t *current, mm_context_t *new);
#else
static inline void mm_context_swap(mm_context_t *current, mm_context_t *new)
{
	mm_context_swap_private(current, new);
}
#endif /* HAVE_TSAN */
