
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
	fiber->condition = 0;
	fiber->condition_status = 0;
	mm_fiber_op_end(fiber);
	mm_listinit(&fiber->waiters);
	mm_listinit(&fiber->link);
	mm_listinit(&fiber->link_wait);
}

mmfiber*
mm_fiber_alloc(int stack_size)
{
	mmfiber *fiber = malloc(sizeof(mmfiber));
	if (fiber == NULL)
		return NULL;
	mm_fiber_init(fiber);
	fiber->context = mm_context_alloc(stack_size);
	if (fiber->context == NULL) {
		free(fiber);
		return NULL;
	}
	return fiber;
}

void
mm_fiber_free(mmfiber *fiber)
{
	if (fiber->context)
		mm_context_free(fiber->context);
	free(fiber);
}
