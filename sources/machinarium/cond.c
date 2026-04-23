
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>

#include <machinarium/machinarium.h>
#include <machinarium/cond.h>
#include <machinarium/machine.h>

MACHINE_API machine_cond_t *machine_cond_create(void)
{
	mm_cond_t *cond = mm_malloc(sizeof(mm_cond_t));
	if (cond == NULL) {
		mm_errno_set(ENOMEM);
		return NULL;
	}
	mm_cond_init(cond);
	return (machine_cond_t *)cond;
}

MACHINE_API void machine_cond_free(machine_cond_t *obj)
{
	mm_free(obj);
}

MACHINE_API void machine_cond_propagate(machine_cond_t *obj,
					machine_cond_t *prop)
{
	mm_cond_t *cond = mm_cast(mm_cond_t *, obj);
	mm_cond_t *propagate = mm_cast(mm_cond_t *, prop);
	cond->propagate = propagate;
}

MACHINE_API void machine_cond_signal(machine_cond_t *obj)
{
	mm_cond_t *cond = mm_cast(mm_cond_t *, obj);
	mm_cond_signal(cond);
}

MACHINE_API int machine_cond_wait(machine_cond_t *obj, uint32_t time_ms)
{
	mm_cond_t *cond = mm_cast(mm_cond_t *, obj);
	return mm_cond_wait(cond, time_ms);
}

static inline int already_signalled(mm_cond_t *cond, mm_cond_t **history,
				    size_t depth, size_t max)
{
	if (depth == max) {
		return 1;
	}

	for (size_t i = 0; i < depth; ++i) {
		if (history[i] == cond) {
			return 1;
		}
	}

	return 0;
}

static inline void signal_impl(mm_cond_t *cond, mm_scheduler_t *sched,
			       int propagated, mm_cond_t **history,
			       size_t depth, size_t max)
{
	history[depth++] = cond;

	if (cond->propagate) {
		if (!already_signalled(cond->propagate, history, depth, max)) {
			signal_impl(cond->propagate, sched, 1 /* propagated */,
				    history, depth, max);
		}
	}

	mm_list_t *i;
	mm_list_foreach (&cond->awaiters, i) {
		mm_cond_awaiter_t *awaiter;
		awaiter = mm_container_of(i, mm_cond_awaiter_t, link);
		awaiter->propagated = propagated;
		assert(awaiter->owner == sched);
		mm_scheduler_wakeup(sched, awaiter->call.coroutine);
	}
}

void mm_cond_signal(mm_cond_t *cond)
{
	mm_scheduler_t *sched = &mm_self->scheduler;

#ifndef NDEBUG
	assert(cond->owner == NULL || cond->owner == sched);
#endif

	mm_cond_t *history[32];

	signal_impl(cond, sched, 0, history, 0,
		    sizeof(history) / sizeof(history[0]));
}

int mm_cond_wait(mm_cond_t *cond, uint32_t time_ms)
{
#ifndef NDEBUG
	assert(cond->owner == NULL || cond->owner == &mm_self->scheduler);
	cond->owner = &mm_self->scheduler;
#endif

	mm_errno_set(0);

	mm_cond_awaiter_t awaiter;
	memset(&awaiter, 0, sizeof(mm_cond_awaiter_t));
	mm_list_init(&awaiter.link);

#ifndef NDEBUG
	awaiter.owner = &mm_self->scheduler;
#endif

	mm_list_append(&cond->awaiters, &awaiter.link);

	mm_call(&awaiter.call, MM_CALL_COND, time_ms);

	mm_list_unlink(&awaiter.link);

#ifndef NDEBUG
	cond->owner = NULL;
#endif

	if (awaiter.call.status != 0) {
		return MM_COND_WAIT_FAIL;
	}

	if (awaiter.propagated) {
		return MM_COND_WAIT_OK_PROPAGATED;
	}

	return MM_COND_WAIT_OK;
}
