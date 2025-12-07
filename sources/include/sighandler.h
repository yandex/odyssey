#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium/machinarium.h>

#include <types.h>

void od_system_signal_handler(void *arg);

void od_system_shutdown(od_system_t *system, od_instance_t *instance);
