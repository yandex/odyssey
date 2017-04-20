
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

static void
mm_call_timer_cb(mm_timer_t *handle)
{
	mm_call_t *call = handle->arg;
	call->timedout = 1;
	call->status = ETIMEDOUT;
	if (call->fiber)
		mm_scheduler_wakeup(call->fiber);
}

static void
mm_call_cancel_cb(void *obj, void *arg)
{
	mm_call_t *call = arg;
	(void)obj;
	call->status = ECANCELED;
	if (call->fiber)
		mm_scheduler_wakeup(call->fiber);
}

void
mm_call(mm_call_t *call,
        mm_scheduler_t *scheduler,
        mm_clock_t *clock,
        int time_ms)
{
	mm_fiber_t *fiber;
	fiber = mm_scheduler_current(scheduler);
	if (mm_fiber_is_cancelled(fiber))
		return;

	fiber->call_ptr = call;
	call->fiber = fiber;
	call->active = 1;
	call->cancel_function = mm_call_cancel_cb;
	call->arg = call;
	call->timedout = 0;
	call->status = 0;

	mm_timer_start(clock, &call->timer, mm_call_timer_cb,
	               call, time_ms);
	mm_scheduler_yield(scheduler);
	mm_timer_stop(&call->timer);

	call->active = 0;
	call->cancel_function = NULL;
	call->arg = NULL;
	fiber->call_ptr = NULL;
}

void
mm_call_fast(mm_call_t *call,
             mm_scheduler_t *scheduler,
             void (*function)(void*),
             void *arg)
{
	mm_fiber_t *fiber;
	fiber = mm_scheduler_current(scheduler);
	if (mm_fiber_is_cancelled(fiber))
		return;

	fiber->call_ptr = call;
	call->fiber = fiber;
	call->active = 1;
	call->cancel_function = mm_call_cancel_cb;
	call->arg = call;
	call->timedout = 0;
	call->status = 0;

	function(arg);

	call->active = 0;
	call->cancel_function = NULL;
	call->arg = NULL;
	fiber->call_ptr = NULL;
}
