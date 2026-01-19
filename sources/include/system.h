#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium/machinarium.h>

#include <types.h>
#include <id.h>
#include <config.h>

struct od_system_server {
	machine_io_t *io;
	machine_tls_t *tls;
	od_config_listen_t *config;
	struct addrinfo *addr;
	od_global_t *global;
	od_list_t link;
	od_id_t sid;

	atomic_bool closed;
	volatile bool pre_exited;
};

void od_system_server_free(od_system_server_t *server);
od_system_server_t *od_system_server_init(void);

struct od_system {
	int64_t machine;
	int64_t sighandler_machine;
	od_global_t *global;
};

od_system_t *od_system_create(void);
void od_system_init(od_system_t *);
int od_system_start(od_system_t *, od_global_t *);
void od_system_config_reload(od_system_t *);
void od_system_free(od_system_t *system);
