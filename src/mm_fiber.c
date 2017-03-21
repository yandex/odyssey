
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

void mm_fiber_init(mm_fiber_t *fiber)
{
	memset(fiber, 0, sizeof(mm_fiber_t));
	fiber->id = UINT64_MAX;
	fiber->state = MM_FIBER_NEW;
	fiber->condition = 0;
	fiber->condition_status = 0;
	mm_call_init(&fiber->call);
	mm_list_init(&fiber->waiters);
	mm_list_init(&fiber->link);
	mm_list_init(&fiber->link_wait);
}

mm_fiber_t*
mm_fiber_allocate(int stack_size)
{
	mm_fiber_t *fiber;
	fiber = malloc(sizeof(mm_fiber_t));
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
mm_fiber_free(mm_fiber_t *fiber)
{
	if (fiber->context)
		mm_context_free(fiber->context);
	free(fiber);
}
