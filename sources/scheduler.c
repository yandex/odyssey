
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
	mm_coroutine_t *coroutine = arg;
	mm_scheduler_t *scheduler = &mm_self->scheduler;
	mm_scheduler_set(scheduler, coroutine, MM_CACTIVE);

	coroutine->function(coroutine->function_arg);

	/* wakeup joiners */
	mm_list_t *i;
	mm_list_foreach(&coroutine->joiners, i) {
		mm_coroutine_t *joiner;
		joiner = mm_container_of(i, mm_coroutine_t, link_join);
		mm_scheduler_set(scheduler, joiner, MM_CREADY);
	}

	mm_scheduler_set(scheduler, coroutine, MM_CFREE);
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
	mm_coroutine_init(&scheduler->main);
	scheduler->current      = &scheduler->main;
	return 0;
}

void mm_scheduler_free(mm_scheduler_t *scheduler)
{
	mm_coroutine_t *coroutine;
	mm_list_t *i, *p;
	mm_list_foreach_safe(&scheduler->list_ready, i, p) {
		coroutine = mm_container_of(i, mm_coroutine_t, link);
		mm_coroutine_free(coroutine);
	}
	mm_list_foreach_safe(&scheduler->list_active, i, p) {
		coroutine = mm_container_of(i, mm_coroutine_t, link);
		mm_coroutine_free(coroutine);
	}
	mm_list_foreach_safe(&scheduler->list_free, i, p) {
		coroutine = mm_container_of(i, mm_coroutine_t, link);
		mm_coroutine_free(coroutine);
	}
}

void mm_scheduler_run(mm_scheduler_t *scheduler)
{
	while (scheduler->count_ready > 0)
	{
		mm_coroutine_t *coroutine;
		coroutine = mm_container_of(scheduler->list_ready.next, mm_coroutine_t, link);
		mm_scheduler_set(&mm_self->scheduler, coroutine, MM_CACTIVE);
		mm_scheduler_call(&mm_self->scheduler, coroutine);
	}
}

mm_coroutine_t*
mm_scheduler_new(mm_scheduler_t *scheduler, mm_function_t function, void *arg)
{
	mm_coroutine_t *coroutine;
	if (scheduler->count_free) {
		coroutine = mm_container_of(scheduler->list_free.next, mm_coroutine_t, link);
		assert(coroutine->state == MM_CFREE);
		mm_list_init(&coroutine->link_join);
		mm_list_init(&coroutine->joiners);
		coroutine->cancel = 0;
	} else {
		coroutine = mm_coroutine_allocate(scheduler->size_stack);
		if (coroutine == NULL)
			return NULL;
	}
	mm_context_create(&coroutine->context, &coroutine->stack,
	                  mm_scheduler_main, coroutine);
	coroutine->id = scheduler->id_seq++;
	coroutine->function = function;
	coroutine->function_arg = arg;
	mm_scheduler_set(scheduler, coroutine, MM_CREADY);
	return coroutine;
}

mm_coroutine_t*
mm_scheduler_find(mm_scheduler_t *scheduler, uint64_t id)
{
	mm_coroutine_t *coroutine;
	mm_list_t *i;
	mm_list_foreach(&scheduler->list_ready, i) {
		coroutine = mm_container_of(i, mm_coroutine_t, link);
		if (coroutine->id == id)
			return coroutine;
	}
	mm_list_foreach(&scheduler->list_active, i) {
		coroutine = mm_container_of(i, mm_coroutine_t, link);
		if (coroutine->id == id)
			return coroutine;
	}
	return NULL;
}

void mm_scheduler_set(mm_scheduler_t *scheduler, mm_coroutine_t *coroutine,
                      mm_coroutinestate_t state)
{
	if (coroutine->state == state)
		return;
	switch (coroutine->state) {
	case MM_CNEW: break;
	case MM_CREADY:
		scheduler->count_ready--;
		break;
	case MM_CACTIVE:
		scheduler->count_active--;
		break;
	case MM_CFREE:
		scheduler->count_free--;
		break;
	}
	mm_list_t *target = NULL;
	switch (state) {
	case MM_CNEW: break;
	case MM_CREADY:
		target = &scheduler->list_ready;
		scheduler->count_ready++;
		break;
	case MM_CACTIVE:
		target = &scheduler->list_active;
		scheduler->count_active++;
		break;
	case MM_CFREE:
		target = &scheduler->list_free;
		scheduler->count_free++;
		break;
	}
	mm_list_unlink(&coroutine->link);
	mm_list_init(&coroutine->link);
	mm_list_append(target, &coroutine->link);
	coroutine->state = state;
}

void mm_scheduler_call(mm_scheduler_t *scheduler, mm_coroutine_t *coroutine)
{
	mm_coroutine_t *resume = scheduler->current;
	assert(resume != NULL);
	coroutine->resume = resume;
	scheduler->current = coroutine;
	mm_context_swap(&resume->context, &coroutine->context);
}

void mm_scheduler_yield(mm_scheduler_t *scheduler)
{
	mm_coroutine_t *current = scheduler->current;
	mm_coroutine_t *resume = current->resume;
	assert(resume != NULL);
	scheduler->current = resume;
	mm_context_swap(&current->context, &resume->context);
}

void mm_scheduler_join(mm_coroutine_t *coroutine, mm_coroutine_t *joiner)
{
	mm_list_append(&coroutine->joiners, &joiner->link_join);
}
