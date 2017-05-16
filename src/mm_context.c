
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

#include <ucontext.h>

typedef struct mm_stack_t mm_stack_t;

struct mm_stack_t {
  char   *pointer;
  size_t  size;
};

static inline int
mm_stack_create(mm_stack_t *stack, size_t size)
{
	stack->size = ((size * sizeof (void*) + 4096 - 1) / 4096) * 4096;
	stack->pointer = malloc(stack->size);
	if (stack->pointer == NULL)
		return -1;
	/*
	stack->valgrind_id =
		VALGRIND_STACK_REGISTER(stack->pointer, stack->pointer + stack->size);
	*/
	return 0;
}

static inline void
mm_stack_free(mm_stack_t *stack)
{
	if (stack->pointer == NULL)
		return;
	/*
	VALGRIND_STACK_DEREGISTER(stack->valgrind_id);
	*/
	free(stack->pointer);
}


typedef struct mm_context_t mm_context_t;

struct mm_context_t {
	mm_stack_t stack;
	ucontext_t context;
};

void *mm_context_alloc(size_t stack_size)
{
	mm_context_t *ctx = malloc(sizeof(mm_context_t));
	if (ctx == NULL)
		return NULL;
	memset(ctx, 0, sizeof(mm_context_t));
	if (stack_size == 0)
		return ctx;
	int rc;
	rc = mm_stack_create(&ctx->stack, stack_size);
	if (rc == -1) {
		free(ctx);
		return NULL;
	}
	return ctx;
}

void mm_context_free(void *ctxp)
{
	mm_context_t *ctx = ctxp;
	mm_stack_free(&ctx->stack);
	free(ctx);
}

void mm_context_create(void *ctxp, void *link, void (*function)(void*), void *arg)
{
	mm_context_t *main_context = link;
	mm_context_t *ctx = ctxp;
	ctx->context.uc_stack.ss_sp   = ctx->stack.pointer;
	ctx->context.uc_stack.ss_size = ctx->stack.size;
	ctx->context.uc_link          = &main_context->context;
	getcontext(&ctx->context);
	makecontext(&ctx->context, (void(*)())function, 1, arg);
}

void mm_context_swap(void *prevp, void *nextp)
{
	mm_context_t *prev = prevp;
	mm_context_t *next = nextp;
	swapcontext(&prev->context, &next->context);
}
