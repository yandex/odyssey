
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium_private.h>

void
mm_context_init_main(mmcontext *c)
{
	memset(c, 0, sizeof(*c));
}

void
mm_context_init(mmcontext *c,
                void *stack,
                int stack_size,
                mmcontext *link,
                mmcontextf function,
                void *arg)
{
	c->context.uc_stack.ss_sp   = stack;
	c->context.uc_stack.ss_size = stack_size;
	c->context.uc_link          = &link->context;
	getcontext(&c->context);
	makecontext(&c->context, (void(*)())function, 1, arg);
}

void mm_context_swap(mmcontext *src, mmcontext *dst)
{
	swapcontext(&src->context, &dst->context);
}
