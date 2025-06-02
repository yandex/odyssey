#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

void od_system_signal_handler(void *arg);

void od_system_shutdown(od_system_t *system, od_instance_t *instance);
