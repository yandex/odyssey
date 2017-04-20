
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

static void
mm_scheduler_main(void *arg)
{
	mm_fiber_t *fiber = arg;
	mm_scheduler_t *scheduler = fiber->scheduler;
	mm_scheduler_set(fiber, MM_FIBER_ACTIVE);

	fiber->function(fiber->function_arg);
	mm_scheduler_wakeup_waiters(fiber);

	mm_scheduler_set(fiber, MM_FIBER_FREE);
	mm_scheduler_yield(scheduler);
}

int mm_scheduler_init(mm_scheduler_t *scheduler, int size_stack, void *data)
{
	mm_list_init(&scheduler->list_ready);
	mm_list_init(&scheduler->list_active);
	mm_list_init(&scheduler->list_free);
	scheduler->id_seq       = 0;
	scheduler->count_ready  = 0;
	scheduler->count_active = 0;
	scheduler->count_free   = 0;
	scheduler->size_stack   = size_stack;
	scheduler->data         = data;
	mm_fiber_init(&scheduler->main);
	scheduler->main.context = mm_context_alloc(0);
	if (scheduler->main.context == NULL)
		return -1;
	mm_context_create(scheduler->main.context, NULL, NULL);
	scheduler->current      = &scheduler->main;
	return 0;
}

void mm_scheduler_free(mm_scheduler_t *scheduler)
{
	mm_fiber_t *fiber;
	mm_list_t *i, *p;
	mm_list_foreach_safe(&scheduler->list_ready, i, p) {
		fiber = mm_container_of(i, mm_fiber_t, link);
		mm_fiber_free(fiber);
	}
	mm_list_foreach_safe(&scheduler->list_active, i, p) {
		fiber = mm_container_of(i, mm_fiber_t, link);
		mm_fiber_free(fiber);
	}
	mm_list_foreach_safe(&scheduler->list_free, i, p) {
		fiber = mm_container_of(i, mm_fiber_t, link);
		mm_fiber_free(fiber);
	}
	if (scheduler->main.context)
		mm_context_free(scheduler->main.context);
}

mm_fiber_t*
mm_scheduler_new(mm_scheduler_t *scheduler, mm_function_t function, void *arg)
{
	mm_fiber_t *fiber;
	if (scheduler->count_free) {
		fiber = mm_container_of(scheduler->list_free.next, mm_fiber_t, link);
		assert(fiber->state == MM_FIBER_FREE);
		mm_list_init(&fiber->link_wait);
		mm_list_init(&fiber->waiters);
		fiber->cancel = 0;
	} else {
		fiber = mm_fiber_allocate(scheduler->size_stack);
		if (fiber == NULL)
			return NULL;
		fiber->scheduler = scheduler;
	}
	mm_context_create(fiber->context, mm_scheduler_main, fiber);
	fiber->id = scheduler->id_seq++;
	fiber->function = function;
	fiber->function_arg = arg;
	fiber->data = NULL;
	mm_scheduler_set(fiber, MM_FIBER_READY);
	return fiber;
}

mm_fiber_t*
mm_scheduler_find(mm_scheduler_t *scheduler, uint64_t id)
{
	mm_fiber_t *fiber;
	mm_list_t *i;
	mm_list_foreach(&scheduler->list_ready, i) {
		fiber = mm_container_of(i, mm_fiber_t, link);
		if (fiber->id == id)
			return fiber;
	}
	mm_list_foreach(&scheduler->list_active, i) {
		fiber = mm_container_of(i, mm_fiber_t, link);
		if (fiber->id == id)
			return fiber;
	}
	return NULL;
}

void
mm_scheduler_set(mm_fiber_t *fiber, mm_fiberstate_t state)
{
	mm_scheduler_t *scheduler = fiber->scheduler;
	if (fiber->state == state)
		return;
	switch (fiber->state) {
	case MM_FIBER_NEW: break;
	case MM_FIBER_READY:
		scheduler->count_ready--;
		break;
	case MM_FIBER_ACTIVE:
		scheduler->count_active--;
		break;
	case MM_FIBER_FREE:
		scheduler->count_free--;
		break;
	}
	mm_list_t *target = NULL;
	switch (state) {
	case MM_FIBER_NEW: break;
	case MM_FIBER_READY:
		target = &scheduler->list_ready;
		scheduler->count_ready++;
		break;
	case MM_FIBER_ACTIVE:
		target = &scheduler->list_active;
		scheduler->count_active++;
		break;
	case MM_FIBER_FREE:
		target = &scheduler->list_free;
		scheduler->count_free++;
		break;
	}
	mm_list_unlink(&fiber->link);
	mm_list_init(&fiber->link);
	mm_list_append(target, &fiber->link);
	fiber->state = state;
}

void
mm_scheduler_call(mm_fiber_t *fiber)
{
	mm_scheduler_t *scheduler = fiber->scheduler;
	mm_fiber_t *resume = scheduler->current;
	assert(resume != NULL);
	fiber->resume = resume;
	scheduler->current = fiber;
	mm_context_swap(resume->context, fiber->context);
}

void
mm_scheduler_yield(mm_scheduler_t *scheduler)
{
	mm_fiber_t *current = scheduler->current;
	mm_fiber_t *resume = current->resume;
	assert(resume != NULL);
	scheduler->current = resume;
	mm_context_swap(current->context, resume->context);
}

void mm_scheduler_wait(mm_fiber_t *fiber, mm_fiber_t *waiter)
{
	mm_list_append(&fiber->waiters, &waiter->link_wait);
}
