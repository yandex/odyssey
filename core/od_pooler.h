#ifndef OD_POOLER_H_
#define OD_POOLER_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_pooler_t od_pooler_t;

struct od_pooler_t {
	mm_t             env;
	mm_io_t          server;
	od_routepool_t   route_pool;
	od_clientlist_t  client_list;
	uint64_t         client_seq;
	od_t            *od;
};

int od_pooler_init(od_pooler_t*, od_t*);
int od_pooler_start(od_pooler_t*);

#endif
