
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
	fiber->call_ptr = NULL;
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
	int rc;
	rc = mm_fiberstack_create(&fiber->stack, stack_size);
	if (rc == -1) {
		free(fiber);
		return NULL;
	}
	fiber->context = mm_context_alloc();
	if (fiber->context == NULL) {
		mm_fiberstack_free(&fiber->stack);
		free(fiber);
		return NULL;
	}
	return fiber;
}

void
mm_fiber_free(mm_fiber_t *fiber)
{
	mm_fiberstack_free(&fiber->stack);
	if (fiber->context)
		mm_context_free(fiber->context);
	free(fiber);
}

void
mm_fiber_cancel(mm_fiber_t *fiber)
{
	if (fiber->cancel)
		return;
	fiber->cancel++;
	if (fiber->call_ptr)
		mm_call_cancel(fiber->call_ptr, fiber);
}
