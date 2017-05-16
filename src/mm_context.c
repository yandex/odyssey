
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

#include <ucontext.h>

typedef struct mm_context_t mm_context_t;

struct mm_context_t {
	ucontext_t context;
};

void *mm_context_alloc(void)
{
	mm_context_t *ctx = malloc(sizeof(mm_context_t));
	if (ctx == NULL)
		return NULL;
	memset(ctx, 0, sizeof(mm_context_t));
	return ctx;
}

void mm_context_free(void *ctxp)
{
	mm_context_t *ctx = ctxp;
	free(ctx);
}

void
mm_context_create(void *ctxp, void *link, mm_fiberstack_t *stack,
                  void (*function)(void*),
                  void *arg)
{
	mm_context_t *main_context = link;
	mm_context_t *ctx = ctxp;
	ctx->context.uc_stack.ss_sp   = stack->pointer;
	ctx->context.uc_stack.ss_size = stack->size;
	ctx->context.uc_link          = &main_context->context;
	getcontext(&ctx->context);
	makecontext(&ctx->context, (void(*)())function, 1, arg);
}

void
mm_context_swap(void *prevp, void *nextp)
{
	mm_context_t *prev = prevp;
	mm_context_t *next = nextp;
	swapcontext(&prev->context, &next->context);
}
