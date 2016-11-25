
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium_private.h>

void mm_fiber_init(mmfiber *fiber)
{
	memset(fiber, 0, sizeof(mmfiber));
	fiber->state = MM_FNEW;
	mm_fiber_op_end(fiber);
	mm_listinit(&fiber->waiters);
	mm_listinit(&fiber->link);
	mm_listinit(&fiber->link_wait);
}

mmfiber*
mm_fiber_alloc(int size_stack)
{
	int size = sizeof(mmfiber) + size_stack;
	mmfiber *fiber = malloc(size);
	if (fiber == NULL)
		return NULL;
	mm_fiber_init(fiber);
	memset(mm_fiber_stackof(fiber), 0, size_stack);
	return fiber;
}

void
mm_fiber_free(mmfiber *fiber)
{
	free(fiber);
}
