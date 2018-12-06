#ifndef ODYSSEY_SYSTEM_H
#define ODYSSEY_SYSTEM_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_system_server od_system_server_t;
typedef struct od_system        od_system_t;

struct od_system_server
{
	machine_io_t       *io;
	machine_tls_t      *tls;
	od_config_listen_t *config;
	struct addrinfo    *addr;
	od_global_t        *global;
};

struct od_system
{
	int64_t      machine;
	od_global_t *global;
};

void od_system_init(od_system_t*);
int  od_system_start(od_system_t*, od_global_t*);

#endif /* ODYSSEY_SYSTEM_H */
