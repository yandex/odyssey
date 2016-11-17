
/*
 * flint.
 *
 * Cooperative multitasking engine.
*/

#include <flint_private.h>

void ft_fiber_init(ftfiber *fiber)
{
	memset(fiber, 0, sizeof(ftfiber));
	fiber->state = FT_FNEW;
	ft_fiber_opend(fiber);
	ft_listinit(&fiber->waiters);
	ft_listinit(&fiber->link);
	ft_listinit(&fiber->link_wait);
}

ftfiber*
ft_fiber_alloc(int size_stack)
{
	int size = sizeof(ftfiber) + size_stack;
	ftfiber *fiber = malloc(size);
	if (fiber == NULL)
		return NULL;
	ft_fiber_init(fiber);
	memset(ft_fiber_stackof(fiber), 0, size_stack);
	return fiber;
}

void
ft_fiber_free(ftfiber *fiber)
{
	free(fiber);
}
