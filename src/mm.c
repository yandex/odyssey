
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

static void
mm_idle_cb(mm_idle_t *handle)
{
	mm_t *machine = handle->arg;
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
	int rc;
	rc = mm_loop_init(&machine->loop);
	if (rc < 0)
		return NULL;
	mm_loop_set_idle(&machine->loop, mm_idle_cb, machine);
	return machine;
}

MACHINE_API int
machine_free(machine_t obj)
{
	mm_t *machine = obj;
	if (machine->online)
		return -1;
	mm_loop_shutdown(&machine->loop);
	mm_scheduler_free(&machine->scheduler);
	free(obj);
	return 0;
}

MACHINE_API int64_t
machine_create_fiber(machine_t obj,
                     machine_fiber_function_t function,
                     void *arg)
{
	mm_t *machine = obj;
	mm_fiber_t *fiber;
	fiber = mm_scheduler_new(&machine->scheduler, function, arg);
	if (fiber == NULL)
		return -1;
	fiber->data = machine;
	return fiber->id;
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
		mm_loop_step(&machine->loop);
	}
}

MACHINE_API void
machine_stop(machine_t obj)
{
	mm_t *machine = obj;
	machine->online = 0;
}

MACHINE_API void
machine_sleep(machine_t obj, uint64_t time_ms)
{
	mm_t *machine = obj;
	mm_fiber_t *fiber;
	fiber = mm_scheduler_current(&machine->scheduler);
	if (mm_fiber_is_cancelled(fiber))
		return;
	mm_call_t call;
	mm_call(&call, &machine->scheduler, &machine->loop.clock, time_ms);
}

MACHINE_API int
machine_wait(machine_t obj, uint64_t id)
{
	mm_t *machine = obj;
	mm_fiber_t *fiber = mm_scheduler_find(&machine->scheduler, id);
	if (fiber == NULL)
		return -1;
	mm_fiber_t *waiter = mm_scheduler_current(&machine->scheduler);
	mm_scheduler_wait(fiber, waiter);
	mm_scheduler_yield(&machine->scheduler);
	return 0;
}

MACHINE_API int
machine_cancel(machine_t obj, uint64_t id)
{
	mm_t *machine = obj;
	mm_fiber_t *fiber = mm_scheduler_find(&machine->scheduler, id);
	if (fiber == NULL)
		return -1;
	mm_fiber_cancel(fiber);
	return 0;
}

MACHINE_API int
machine_condition(machine_t obj, uint64_t time_ms)
{
	mm_t *machine = obj;
	mm_fiber_t *fiber = mm_scheduler_current(&machine->scheduler);
	if (mm_fiber_is_cancelled(fiber))
		return -1;
	mm_call_t call;
	mm_call(&call, &machine->scheduler, &machine->loop.clock, time_ms);
	if (call.status != 0)
		return -1;
	return 0;
}

MACHINE_API int
machine_signal(machine_t obj, uint64_t id)
{
	mm_t *machine = obj;
	mm_fiber_t *fiber = mm_scheduler_find(&machine->scheduler, id);
	if (fiber == NULL)
		return -1;
	mm_call_t *call = fiber->call_ptr;
	if (call == NULL)
		return -1;
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

MACHINE_API int
machinarium_init(void)
{
	mm_tls_init();
	return 0;
}

MACHINE_API void
machinarium_free(void)
{ }
