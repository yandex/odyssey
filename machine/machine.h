#ifndef MACHINE_H_
#define MACHINE_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#if __GNUC__ >= 4
#  define MACHINE_API __attribute__((visibility("default")))
#else
#  define MACHINE_API
#endif

typedef void (*machine_fiber_function_t)(void *arg);

typedef void* machine_t;
typedef void* machine_fiber_t;
typedef void* machine_io_t;

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

MACHINE_API void
machine_wait(machine_fiber_t);

MACHINE_API void
machine_cancel(machine_fiber_t);

MACHINE_API int
machine_condition(machine_t, uint64_t time_ms);

MACHINE_API int
machine_signal(machine_fiber_t);

MACHINE_API int
machine_is_active(machine_t);

MACHINE_API int
machine_is_cancelled(machine_t);

#endif
