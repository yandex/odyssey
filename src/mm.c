
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

static void
mm_prepare_cb(uv_prepare_t *handle)
{
	mm_t *machine = handle->data;
	while (machine->scheduler.count_ready > 0) {
		mm_fiber_t *fiber;
		fiber = mm_scheduler_next_ready(&machine->scheduler);
		mm_scheduler_set(fiber, MM_FIBER_ACTIVE);
		mm_scheduler_call(fiber);
	}
}

MACHINE_API machine_t
machine_create(void)
{
	mm_t *machine;
	machine = malloc(sizeof(*machine));
	if (machine == NULL)
		return NULL;
	machine->online = 0;
	mm_scheduler_init(&machine->scheduler, 2048 /* 16K */, machine);
	int rc = uv_loop_init(&machine->loop);
	if (rc < 0)
		return NULL;
	uv_prepare_init(&machine->loop, &machine->prepare);
	machine->prepare.data = machine;
	uv_prepare_start(&machine->prepare, mm_prepare_cb);
	return machine;
}

static void
mm_free_cb(uv_handle_t *handle, void *arg)
{
	(void)handle;
	(void)arg;
	/* make sure we have not leaked anything */
	abort();
}

MACHINE_API int
machine_free(machine_t obj)
{
	mm_t *machine = obj;
	if (machine->online)
		return -1;
	/* close prepare and wait for completion */
	uv_close((uv_handle_t*)&machine->prepare, NULL);
	uv_run(&machine->loop, UV_RUN_DEFAULT);
	uv_walk(&machine->loop, mm_free_cb, NULL);
	uv_run(&machine->loop, UV_RUN_DEFAULT);
	uv_stop(&machine->loop);
	uv_loop_close(&machine->loop);
	mm_scheduler_free(&machine->scheduler);
	free(obj);
	return 0;
}

MACHINE_API machine_fiber_t
machine_create_fiber(machine_t obj,
                     machine_fiber_function_t function,
                     void *arg)
{
	mm_t *machine = obj;
	mm_fiber_t *fiber;
	fiber = mm_scheduler_new(&machine->scheduler, function, arg);
	if (fiber == NULL)
		return NULL;
	uv_timer_init(&machine->loop, &fiber->timer);
	fiber->timer.data = fiber;
	fiber->data = machine;
	return fiber;
}

MACHINE_API void
machine_start(machine_t obj)
{
	mm_t *machine = obj;
	if (machine->online)
		return;
	machine->online = 1;
	for (;;) {
		if (! machine->online) {
			if (! mm_scheduler_online(&machine->scheduler))
				break;
		}
		uv_run(&machine->loop, UV_RUN_ONCE);
	}
}

MACHINE_API void
machine_stop(machine_t obj)
{
	mm_t *machine = obj;
	machine->online = 0;
}

static void
mm_sleep_timer_cb(uv_timer_t *handle)
{
	mm_fiber_t *fiber = handle->data;
	mm_scheduler_wakeup(fiber);
}

static inline void
mm_sleep_cancel_cb(void *obj, void *arg)
{
	(void)arg;
	mm_fiber_t *fiber = obj;
	uv_timer_stop(&fiber->timer);
	uv_handle_t *handle = (uv_handle_t*)&fiber->timer;
	if (! uv_is_closing(handle))
		uv_close(handle, NULL);
	mm_scheduler_wakeup(fiber);
}

MACHINE_API void
machine_sleep(machine_t obj, uint64_t time_ms)
{
	mm_t *machine = obj;
	mm_fiber_t *fiber;
	fiber = mm_scheduler_current(&machine->scheduler);
	if (mm_fiber_is_cancelled(fiber))
		return;
	uv_timer_start(&fiber->timer, mm_sleep_timer_cb, time_ms, 0);
	mm_call_begin(&fiber->call, mm_sleep_cancel_cb, NULL);
	mm_scheduler_yield(&machine->scheduler);
	mm_call_end(&fiber->call);
}

MACHINE_API void
machine_wait(machine_fiber_t obj)
{
	mm_fiber_t *fiber = obj;
	mm_t *machine = fiber->data;
	mm_fiber_t *waiter = mm_scheduler_current(&machine->scheduler);
	mm_scheduler_wait(fiber, waiter);
	mm_scheduler_yield(&machine->scheduler);
}

MACHINE_API void
machine_cancel(machine_fiber_t obj)
{
	mm_fiber_cancel(obj);
}

static void
mm_condition_timer_cb(uv_timer_t *handle)
{
	mm_fiber_t *fiber = handle->data;
	assert(fiber->condition);
	fiber->condition_status = -ETIMEDOUT;
	mm_scheduler_wakeup(fiber);
}
static inline void
mm_condition_cancel_cb(void *obj, void *arg)
{
	(void)arg;
	mm_fiber_t *fiber = obj;
	uv_timer_stop(&fiber->timer);
	uv_handle_t *handle = (uv_handle_t*)&fiber->timer;
	if (! uv_is_closing(handle))
		uv_close(handle, NULL);
	assert(fiber->condition);
	fiber->condition_status = -ECANCELED;
	mm_scheduler_wakeup(fiber);
}

MACHINE_API int
machine_condition(machine_t obj, uint64_t time_ms)
{
	mm_t *machine = obj;
	mm_fiber_t *fiber = mm_scheduler_current(&machine->scheduler);
	if (mm_fiber_is_cancelled(fiber))
		return -ECANCELED;
	fiber->condition = 1;
	fiber->condition_status = -1;
	uv_timer_start(&fiber->timer, mm_condition_timer_cb, time_ms, 0);
	mm_call_begin(&fiber->call, mm_condition_cancel_cb, NULL);
	mm_scheduler_yield(&machine->scheduler);
	mm_call_end(&fiber->call);
	fiber->condition = 0;
	return fiber->condition_status;
}

MACHINE_API int
machine_signal(machine_fiber_t obj)
{
	mm_fiber_t *fiber = obj;
	if (! fiber->condition)
		return -1;
	uv_timer_stop(&fiber->timer);
	uv_handle_t *handle = (uv_handle_t*)&fiber->timer;
	if (! uv_is_closing(handle))
		uv_close(handle, NULL);
	fiber->condition_status = 0;
	mm_scheduler_wakeup(fiber);
	return 0;
}

MACHINE_API int
machine_active(machine_t obj)
{
	mm_t *machine = obj;
	return machine->online;
}

MACHINE_API int
machine_cancelled(machine_t obj)
{
	mm_t *machine = obj;
	mm_fiber_t *fiber;
	fiber = mm_scheduler_current(&machine->scheduler);
	if (fiber == NULL)
		return -1;
	return fiber->cancel > 0;
}
