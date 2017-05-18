
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

__thread mm_machine_t *mm_self = NULL;

static void
mm_idle_cb(mm_idle_t *handle)
{
	mm_machine_t *machine = handle->arg;
	while (machine->scheduler.count_ready > 0) {
		mm_fiber_t *fiber;
		fiber = mm_scheduler_next_ready(&machine->scheduler);
		mm_scheduler_set(fiber, MM_FIBER_ACTIVE);
		mm_scheduler_call(fiber);
	}
}

static inline void
machine_free(mm_machine_t *machine)
{
	mm_loop_shutdown(&machine->loop);
	mm_scheduler_free(&machine->scheduler);
}

static void*
machine_main(void *arg)
{
	mm_machine_t *machine = arg;
	mm_self = machine;

	/* set thread name */
	if (machine->name)
		mm_thread_set_name(&machine->thread, machine->name);

	/* create main fiber */
	int64_t id;
	id = machine_fiber_create(machine->main, machine->main_arg);
	(void)id;

	/* run main loop */
	machine->online = 1;
	for (;;) {
		if (! mm_scheduler_online(&machine->scheduler))
			break;
		mm_loop_step(&machine->loop);
	}

	machine->online = 0;
	machine_free(machine);
	return NULL;
}

MACHINE_API int
machine_create(char *name, machine_function_t function, void *arg)
{
	mm_machine_t *machine;
	machine = malloc(sizeof(*machine));
	if (machine == NULL)
		return -1;
	machine->online = 0;
	machine->id = 0;
	machine->main = function;
	machine->main_arg = arg;
	machine->name = NULL;
	if (name) {
		machine->name = strdup(name);
		if (machine->name == NULL) {
			free(machine);
			return -1;
		}
	}
	mm_list_init(&machine->link);
	mm_scheduler_init(&machine->scheduler, 2048 /* 16K */, machine);
	int rc;
	rc = mm_loop_init(&machine->loop);
	if (rc < 0) {
		mm_scheduler_free(&machine->scheduler);
		free(machine);
		return -1;
	}
	mm_loop_set_idle(&machine->loop, mm_idle_cb, machine);
	mm_machinemgr_add(&machinarium.machine_mgr, machine);
	rc = mm_thread_create(&machine->thread, machine_main, machine);
	if (rc == -1) {
		mm_machinemgr_delete(&machinarium.machine_mgr, machine);
		mm_loop_shutdown(&machine->loop);
		mm_scheduler_free(&machine->scheduler);
		free(machine);
		return -1;
	}
	return machine->id;
}

MACHINE_API int
machine_wait(int id)
{
	mm_machine_t *machine;
	machine = mm_machinemgr_delete_by_id(&machinarium.machine_mgr, id);
	if (machine == NULL)
		return -1;
	int rc;
	rc = mm_thread_join(&machine->thread);
	if (machine->name)
		free(machine->name);
	free(machine);
	return rc;
}

MACHINE_API int
machine_active(void)
{
	return mm_self->online;
}

MACHINE_API machine_t
machine_self(void)
{
	return mm_self;
}

MACHINE_API void
machine_stop(void)
{
	mm_self->online = 0;
}

MACHINE_API int64_t
machine_fiber_create(machine_function_t function, void *arg)
{
	mm_fiber_t *fiber;
	fiber = mm_scheduler_new(&mm_self->scheduler, function, arg);
	if (fiber == NULL)
		return -1;
	fiber->data = mm_self;
	return fiber->id;
}

MACHINE_API void
machine_sleep(uint64_t time_ms)
{
	mm_fiber_t *fiber;
	fiber = mm_scheduler_current(&mm_self->scheduler);
	if (mm_fiber_is_cancelled(fiber))
		return;
	mm_call_t call;
	mm_call(&call, &mm_self->scheduler, &mm_self->loop.clock, time_ms);
}

MACHINE_API int
machine_join(uint64_t id)
{
	mm_fiber_t *fiber;
	fiber = mm_scheduler_find(&mm_self->scheduler, id);
	if (fiber == NULL)
		return -1;
	mm_fiber_t *waiter = mm_scheduler_current(&mm_self->scheduler);
	mm_scheduler_wait(fiber, waiter);
	mm_scheduler_yield(&mm_self->scheduler);
	return 0;
}

MACHINE_API int
machine_cancel(uint64_t id)
{
	mm_fiber_t *fiber;
	fiber = mm_scheduler_find(&mm_self->scheduler, id);
	if (fiber == NULL)
		return -1;
	mm_fiber_cancel(fiber);
	return 0;
}

MACHINE_API int
machine_condition(uint64_t time_ms)
{
	mm_fiber_t *fiber;
	fiber = mm_scheduler_current(&mm_self->scheduler);
	if (mm_fiber_is_cancelled(fiber))
		return -1;
	mm_call_t call;
	mm_call(&call, &mm_self->scheduler, &mm_self->loop.clock, time_ms);
	if (call.status != 0)
		return -1;
	return 0;
}

MACHINE_API int
machine_signal(uint64_t id)
{
	mm_fiber_t *fiber;
	fiber = mm_scheduler_find(&mm_self->scheduler, id);
	if (fiber == NULL)
		return -1;
	mm_call_t *call = fiber->call_ptr;
	if (call == NULL)
		return -1;
	mm_scheduler_wakeup(fiber);
	return 0;
}

MACHINE_API int
machine_cancelled(void)
{
	mm_fiber_t *fiber;
	fiber = mm_scheduler_current(&mm_self->scheduler);
	if (fiber == NULL)
		return -1;
	return fiber->cancel > 0;
}
