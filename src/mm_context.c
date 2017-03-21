
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machine_private.h>

#define CORO_ASM 1
#include "coro.h"
#include "coro.c"

typedef struct mm_context_t mm_context_t;

struct mm_context_t {
	struct coro_context context;
	struct coro_stack stack;
};

void *mm_context_alloc(size_t stack_size)
{
	mm_context_t *ctx = malloc(sizeof(mm_context_t));
	if (ctx == NULL)
		return NULL;
	memset(&ctx->context, 0, sizeof(ctx->context));
	if (stack_size == 0)
		return ctx;
	int rc;
	rc = coro_stack_alloc(&ctx->stack, stack_size);
	if (! rc) {
		free(ctx);
		return NULL;
	}
	return ctx;
}

void mm_context_free(void *ctxp)
{
	mm_context_t *ctx = ctxp;
	coro_stack_free(&ctx->stack);
	free(ctx);
}

void mm_context_create(void *ctxp, void (*function)(void*), void *arg)
{
	mm_context_t *ctx = ctxp;
	coro_create(&ctx->context, function, arg,
	            ctx->stack.sptr,
	            ctx->stack.ssze);
}

void mm_context_swap(void *prevp, void *nextp)
{
	mm_context_t *prev = prevp;
	mm_context_t *next = nextp;
	coro_transfer(&prev->context, &next->context);
}
