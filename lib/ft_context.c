
/*
 * flint.
 *
 * Cooperative multitasking engine.
*/

#include <flint_private.h>

void
ft_context_init_main(ftcontext *c)
{
	memset(c, 0, sizeof(*c));
}

void
ft_context_init(ftcontext *c,
                void *stack,
                int stack_size,
                ftcontext *link,
                ftcontextf function,
                void *arg)
{
	c->context.uc_stack.ss_sp   = stack;
	c->context.uc_stack.ss_size = stack_size;
	c->context.uc_link          = &link->context;
	getcontext(&c->context);
	makecontext(&c->context, (void(*)())function, 1, arg);
}

void ft_context_swap(ftcontext *src, ftcontext *dst)
{
	swapcontext(&src->context, &dst->context);
}
