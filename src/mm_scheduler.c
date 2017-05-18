
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

static void
mm_scheduler_main(void *arg)
{
	mm_fiber_t *fiber = arg;

	mm_scheduler_t *scheduler = &mm_self->scheduler;
	mm_scheduler_set(scheduler, fiber, MM_FIBER_ACTIVE);

	fiber->function(fiber->function_arg);

	/* wakeup joiners */
	mm_list_t *i;
	mm_list_foreach(&fiber->joiners, i) {
		mm_fiber_t *joiner;
		joiner = mm_container_of(i, mm_fiber_t, link_join);
		mm_scheduler_set(scheduler, joiner, MM_FIBER_READY);
	}

	mm_scheduler_set(scheduler, fiber, MM_FIBER_FREE);
	mm_scheduler_yield(scheduler);
}

int mm_scheduler_init(mm_scheduler_t *scheduler, int size_stack)
{
	mm_list_init(&scheduler->list_ready);
	mm_list_init(&scheduler->list_active);
	mm_list_init(&scheduler->list_free);
	scheduler->id_seq       = 0;
	scheduler->count_ready  = 0;
	scheduler->count_active = 0;
	scheduler->count_free   = 0;
	scheduler->size_stack   = size_stack;
	mm_fiber_init(&scheduler->main);
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
}

void mm_scheduler_run(mm_scheduler_t *scheduler)
{
	while (scheduler->count_ready > 0)
	{
		mm_fiber_t *fiber;
		fiber = mm_container_of(scheduler->list_ready.next, mm_fiber_t, link);
		mm_scheduler_set(&mm_self->scheduler, fiber, MM_FIBER_ACTIVE);
		mm_scheduler_call(&mm_self->scheduler, fiber);
	}
}

mm_fiber_t*
mm_scheduler_new(mm_scheduler_t *scheduler, mm_function_t function, void *arg)
{
	mm_fiber_t *fiber;
	if (scheduler->count_free) {
		fiber = mm_container_of(scheduler->list_free.next, mm_fiber_t, link);
		assert(fiber->state == MM_FIBER_FREE);
		mm_list_init(&fiber->link_join);
		mm_list_init(&fiber->joiners);
		fiber->cancel = 0;
	} else {
		fiber = mm_fiber_allocate(scheduler->size_stack);
		if (fiber == NULL)
			return NULL;
	}
	mm_context_create(&fiber->context, &fiber->stack,
	                  mm_scheduler_main, fiber);
	fiber->id = scheduler->id_seq++;
	fiber->function = function;
	fiber->function_arg = arg;
	mm_scheduler_set(scheduler, fiber, MM_FIBER_READY);
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

void mm_scheduler_set(mm_scheduler_t *scheduler, mm_fiber_t *fiber,
					  mm_fiberstate_t state)
{
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

void mm_scheduler_call(mm_scheduler_t *scheduler, mm_fiber_t *fiber)
{
	mm_fiber_t *resume = scheduler->current;
	assert(resume != NULL);
	fiber->resume = resume;
	scheduler->current = fiber;
	mm_context_swap(&resume->context, &fiber->context);
}

void mm_scheduler_yield(mm_scheduler_t *scheduler)
{
	mm_fiber_t *current = scheduler->current;
	mm_fiber_t *resume = current->resume;
	assert(resume != NULL);
	scheduler->current = resume;
	mm_context_swap(&current->context, &resume->context);
}

void mm_scheduler_join(mm_fiber_t *fiber, mm_fiber_t *joiner)
{
	mm_list_append(&fiber->joiners, &joiner->link_join);
}
