#ifndef OD_SYSTEM_H
#define OD_SYSTEM_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_system       od_system_t;
typedef struct od_systemserver od_systemserver_t;

struct od_systemserver
{
	od_configlisten_t *config;
	machine_io_t      *io;
	machine_tls_t     *tls;
	struct addrinfo   *addr;
	od_global_t       *global;
};

struct od_system
{
	int64_t     machine;
	od_global_t global;
};

int od_system_init(od_system_t*);
int od_system_start(od_system_t*);

#endif /* OD_SYSTEM_H */
