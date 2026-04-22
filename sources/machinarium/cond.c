
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
	mm_cond_signal(cond, &mm_self->scheduler);
}

MACHINE_API int machine_cond_wait(machine_cond_t *obj, uint32_t time_ms)
{
	mm_cond_t *cond = mm_cast(mm_cond_t *, obj);
	mm_errno_set(0);
	if (cond->call.type != MM_CALL_NONE) {
		mm_errno_set(EINPROGRESS);
		return -1;
	}
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
	cond->propagated = propagated;
	if (cond->call.type == MM_CALL_COND) {
		mm_scheduler_wakeup(sched, cond->call.coroutine);
	}
}

void mm_cond_signal(mm_cond_t *cond, mm_scheduler_t *sched)
{
	mm_cond_t *history[32];

	signal_impl(cond, sched, 0, history, 0,
		    sizeof(history) / sizeof(history[0]));
}
