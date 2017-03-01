
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium_private.h>

#define CORO_ASM 1
#include "coro.h"
#include "coro.c"

typedef struct mmcontext mmcontext;

struct mmcontext {
	struct coro_context context;
	struct coro_stack stack;
};

void *mm_context_alloc(size_t stack_size)
{
	mmcontext *ctx = malloc(sizeof(mmcontext));
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
	mmcontext *ctx = ctxp;
	coro_stack_free(&ctx->stack);
	free(ctx);
}

void mm_context_create(void *ctxp, void (*function)(void*), void *arg)
{
	mmcontext *ctx = ctxp;
	coro_create(&ctx->context, function, arg,
	            ctx->stack.sptr,
	            ctx->stack.ssze);
}

void mm_context_swap(void *prevp, void *nextp)
{
	mmcontext *prev = prevp;
	mmcontext *next = nextp;
	coro_transfer(&prev->context, &next->context);
}
