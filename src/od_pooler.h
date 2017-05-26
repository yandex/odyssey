#ifndef OD_POOLER_H
#define OD_POOLER_H

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_pooler od_pooler_t;

struct od_pooler
{
	int64_t          machine;
	machine_io_t     server;
	machine_tls_t    tls;
	uint64_t         client_seq;
	od_instance_t   *instance;
	machine_queue_t  task_queue;
};

int od_pooler_init(od_pooler_t*, od_instance_t*);
int od_pooler_start(od_pooler_t*);

#endif /* OD_INSTANCE_H */
