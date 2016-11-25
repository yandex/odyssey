
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium_private.h>

static void
mm_scheduler_main(void *arg)
{
	mmfiber *fiber = arg;
	mmscheduler *s = fiber->scheduler;
	mm *f = fiber->data;
	mm_scheduler_set(fiber, MM_FACTIVE);

	fiber->function(fiber->arg);
	mm_wakeup_waiters(f, fiber);

	mm_fiber_timer_stop(fiber);
	mm_scheduler_set(fiber, MM_FFREE);
	mm_scheduler_yield(s);
}

void mm_scheduler_init(mmscheduler *s, int size_stack, void *data)
{
	mm_listinit(&s->list_ready);
	mm_listinit(&s->list_active);
	mm_listinit(&s->list_free);
	s->count_ready  = 0;
	s->count_active = 0;
	s->count_free   = 0;
	s->size_stack   = size_stack;
	s->data         = data;
	s->seq          = 1;
	mm_fiber_init(&s->main);
	mm_context_init_main(&s->main.context);
	s->current      = &s->main;
}

void mm_scheduler_free(mmscheduler *s)
{
	mmfiber *fiber;
	mmlist *i, *p;
	mm_listforeach_safe(&s->list_ready, i, p) {
		fiber = mm_container_of(i, mmfiber, link);
		mm_fiber_free(fiber);
	}
	mm_listforeach_safe(&s->list_active, i, p) {
		fiber = mm_container_of(i, mmfiber, link);
		mm_fiber_free(fiber);
	}
	mm_listforeach_safe(&s->list_free, i, p) {
		fiber = mm_container_of(i, mmfiber, link);
		mm_fiber_free(fiber);
	}
}

mmfiber*
mm_scheduler_new(mmscheduler *s, mmfiberf function, void *arg)
{
	mmfiber *fiber;
	if (s->count_free) {
		fiber = mm_container_of(s->list_free.next, mmfiber, link);
		assert(fiber->state == MM_FFREE);
		mm_listinit(&fiber->link_wait);
		mm_listinit(&fiber->waiters);
		mm_fiber_op_end(fiber);
		fiber->cancel = 0;
	} else {
		fiber = mm_fiber_alloc(s->size_stack);
		if (fiber == NULL)
			return NULL;
		fiber->scheduler = s;
	}
	mm_context_init(&fiber->context,
	                mm_fiber_stackof(fiber),
	                s->size_stack,
	                &s->main.context,
	                mm_scheduler_main,
	                fiber);
	fiber->id = s->seq++;
	fiber->function = function;
	fiber->arg  = arg;
	fiber->data = NULL;
	mm_scheduler_set(fiber, MM_FREADY);
	return fiber;
}

mmfiber*
mm_scheduler_match(mmscheduler *s, uint64_t id)
{
	mmfiber *fiber;
	mmlist *i;
	mm_listforeach(&s->list_active, i) {
		fiber = mm_container_of(i, mmfiber, link);
		if (fiber->id == id)
			return fiber;
	}
	mm_listforeach(&s->list_ready, i) {
		fiber = mm_container_of(i, mmfiber, link);
		if (fiber->id == id)
			return fiber;
	}
	return NULL;
}

void
mm_scheduler_set(mmfiber *fiber, mmfiberstate state)
{
	mmscheduler *s = fiber->scheduler;
	if (fiber->state == state)
		return;
	switch (fiber->state) {
	case MM_FNEW: break;
	case MM_FREADY:
		s->count_ready--;
		break;
	case MM_FACTIVE:
		s->count_active--;
		break;
	case MM_FFREE:
		s->count_free--;
		break;
	}
	mmlist *target = NULL;
	switch (state) {
	case MM_FNEW: break;
	case MM_FREADY:
		target = &s->list_ready;
		s->count_ready++;
		break;
	case MM_FACTIVE:
		target = &s->list_active;
		s->count_active++;
		break;
	case MM_FFREE:
		target = &s->list_free;
		s->count_free++;
		break;
	}
	mm_listunlink(&fiber->link);
	mm_listinit(&fiber->link);
	mm_listappend(target, &fiber->link);
	fiber->state = state;
}

void
mm_scheduler_call(mmfiber *fiber)
{
	mmscheduler *s = fiber->scheduler;
	mmfiber *resume = s->current;
	assert(resume != NULL);
	fiber->resume = resume;
	s->current = fiber;
	mm_context_swap(&resume->context, &fiber->context);
}

void
mm_scheduler_yield(mmscheduler *s)
{
	mmfiber *current = s->current;
	mmfiber *resume = current->resume;
	assert(resume != NULL);
	s->current = resume;
	mm_context_swap(&current->context, &resume->context);
}

void mm_scheduler_wait(mmfiber *fiber, mmfiber *waiter)
{
	mm_listappend(&fiber->waiters, &waiter->link_wait);
}
