#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

typedef struct {
    od_system_t *system;
    machine_channel_t *channel;
} od_grac_shutdown_worker_arg_t;

void od_grac_shutdown_worker(void *arg);
