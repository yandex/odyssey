#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium/machinarium.h>

#include <types.h>

typedef struct {
	od_system_t *system;
	machine_channel_t *channel;
} od_grac_shutdown_worker_arg_t;

void od_grac_shutdown_worker(void *arg);
