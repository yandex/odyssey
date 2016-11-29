#ifndef OD_POOLER_H_
#define OD_POOLER_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct odpooler_t odpooler_t;

struct odpooler_t {
	mm_t             env;
	mm_io_t          server;
	odroute_pool_t   route_pool;
	odclient_pool_t  client_pool;
	uint64_t         client_seq;
	od_t            *od;
};

int od_pooler_init(odpooler_t*, od_t*);
int od_pooler_start(odpooler_t*);

#endif
