
/*
 * flint.
 *
 * Cooperative multitasking engine.
*/

#include <flint_private.h>

static void
ft_scheduler_main(void *arg)
{
	ftfiber *fiber = arg;
	ftscheduler *s = fiber->scheduler;
	ft *f = fiber->data;
	ft_scheduler_set(fiber, FT_FACTIVE);

	fiber->function(fiber->arg);
	ft_wakeup_waiters(f, fiber);

	ft_fiber_timer_stop(fiber);
	ft_scheduler_set(fiber, FT_FFREE);
	ft_scheduler_yield(s);
}

void ft_scheduler_init(ftscheduler *s, int size_stack, void *data)
{
	ft_listinit(&s->list_ready);
	ft_listinit(&s->list_active);
	ft_listinit(&s->list_free);
	s->count_ready  = 0;
	s->count_active = 0;
	s->count_free   = 0;
	s->size_stack   = size_stack;
	s->data         = data;
	s->seq          = 1;
	ft_fiber_init(&s->main);
	ft_context_init_main(&s->main.context);
	s->current      = &s->main;
}

void ft_scheduler_free(ftscheduler *s)
{
	ftfiber *fiber;
	ftlist *i, *p;
	ft_listforeach_safe(&s->list_ready, i, p) {
		fiber = ft_container_of(i, ftfiber, link);
		ft_fiber_free(fiber);
	}
	ft_listforeach_safe(&s->list_active, i, p) {
		fiber = ft_container_of(i, ftfiber, link);
		ft_fiber_free(fiber);
	}
	ft_listforeach_safe(&s->list_free, i, p) {
		fiber = ft_container_of(i, ftfiber, link);
		ft_fiber_free(fiber);
	}
}

ftfiber*
ft_scheduler_new(ftscheduler *s, ftfiberf function, void *arg)
{
	ftfiber *fiber;
	if (s->count_free) {
		fiber = ft_container_of(s->list_free.next, ftfiber, link);
		assert(fiber->state == FT_FFREE);
		ft_listinit(&fiber->link_wait);
		ft_listinit(&fiber->waiters);
		ft_fiber_opend(fiber);
	} else {
		fiber = ft_fiber_alloc(s->size_stack);
		if (fiber == NULL)
			return NULL;
		fiber->scheduler = s;
	}
	ft_context_init(&fiber->context,
	                ft_fiber_stackof(fiber),
	                s->size_stack,
	                &s->main.context,
	                ft_scheduler_main,
	                fiber);
	fiber->id = s->seq++;
	fiber->function = function;
	fiber->arg  = arg;
	fiber->data = NULL;
	ft_scheduler_set(fiber, FT_FREADY);
	return fiber;
}

ftfiber*
ft_scheduler_match(ftscheduler *s, uint64_t id)
{
	ftfiber *fiber;
	ftlist *i;
	ft_listforeach(&s->list_active, i) {
		fiber = ft_container_of(i, ftfiber, link);
		if (fiber->id == id)
			return fiber;
	}
	ft_listforeach(&s->list_ready, i) {
		fiber = ft_container_of(i, ftfiber, link);
		if (fiber->id == id)
			return fiber;
	}
	return NULL;
}

void
ft_scheduler_set(ftfiber *fiber, ftfiberstate state)
{
	ftscheduler *s = fiber->scheduler;
	if (fiber->state == state)
		return;
	switch (fiber->state) {
	case FT_FNEW: break;
	case FT_FREADY:
		s->count_ready--;
		break;
	case FT_FACTIVE:
		s->count_active--;
		break;
	case FT_FFREE:
		s->count_free--;
		break;
	}
	ftlist *target = NULL;
	switch (state) {
	case FT_FNEW: break;
	case FT_FREADY:
		target = &s->list_ready;
		s->count_ready++;
		break;
	case FT_FACTIVE:
		target = &s->list_active;
		s->count_active++;
		break;
	case FT_FFREE:
		target = &s->list_free;
		s->count_free++;
		break;
	}
	ft_listunlink(&fiber->link);
	ft_listinit(&fiber->link);
	ft_listappend(target, &fiber->link);
	fiber->state = state;
}

void
ft_scheduler_call(ftfiber *fiber)
{
	ftscheduler *s = fiber->scheduler;
	ftfiber *resume = s->current;
	assert(resume != NULL);
	fiber->resume = resume;
	s->current = fiber;
	ft_context_swap(&resume->context, &fiber->context);
}

void
ft_scheduler_yield(ftscheduler *s)
{
	ftfiber *current = s->current;
	ftfiber *resume = current->resume;
	assert(resume != NULL);
	s->current = resume;
	ft_context_swap(&current->context, &resume->context);
}

void ft_scheduler_wait(ftfiber *fiber, ftfiber *waiter)
{
	ft_listappend(&fiber->waiters, &waiter->link_wait);
}
