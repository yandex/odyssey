#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

typedef struct {
	od_system_t *system;
	machine_wait_flag_t *is_finished;
} od_signal_handler_arg_t;

void od_system_signal_handler(void *arg);

void od_system_shutdown(od_system_t *system, od_instance_t *instance);
