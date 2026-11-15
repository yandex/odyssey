#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <types.h>

int od_bpf_init(od_global_t *global);
void od_bpf_shutdown(void);

int od_bpf_pipe_enter(od_client_t *client, od_server_t *server);
void od_bpf_pipe_exit(od_client_t *client, od_server_t *server);

int od_bpf_pipe_available(void);
