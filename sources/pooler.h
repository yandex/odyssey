#ifndef OD_POOLER_H
#define OD_POOLER_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_pooler       od_pooler_t;
typedef struct od_poolerserver od_poolerserver_t;

struct od_poolerserver
{
	struct addrinfo   *addr;
	od_schemelisten_t *scheme;
	machine_io_t      *io;
	machine_tls_t     *tls;
	od_system_t       *system;
};

struct od_pooler
{
	int64_t          machine;
	struct addrinfo *addr;
	od_system_t      system;
	od_instance_t   *instance;
};

int od_pooler_init(od_pooler_t*, od_instance_t*);
int od_pooler_start(od_pooler_t*);

#endif /* OD_INSTANCE_H */
