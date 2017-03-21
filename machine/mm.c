
/*
 * machinarium.
 * cooperative multitasking engine.
*/

#include <machine_private.h>
#include <machine.h>

MACHINE_API machine_t
machine_init(void);

MACHINE_API int
machine_free(machine_t);

MACHINE_API machine_fiber_t
machine_create_fiber(machine_t, machine_fiber_function_t, void *arg);

MACHINE_API void
machine_start(machine_t);

MACHINE_API void
machine_stop(machine_t);

MACHINE_API void
machine_sleep(machine_t, uint64_t time_ms);

MACHINE_API int
machine_wait(machine_t, machine_fiber_t);

MACHINE_API int
machine_cancel(machine_t, machine_fiber_t);

MACHINE_API int
machine_condition(machine_t, uint64_t time_ms);

MACHINE_API int
machine_signal(machine_t, machine_fiber_t);

MACHINE_API int
machine_is_active(machine_t);

MACHINE_API int
machine_is_cancelled(machine_t);
