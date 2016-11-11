#ifndef OD_ROUTE_POOL_H_
#define OD_ROUTE_POOL_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct odroute_pool_t odroute_pool_t;

struct odroute_pool_t {
	odlist_t list;
	int      count;
};

void od_routepool_init(odroute_pool_t*);
void od_routepool_free(odroute_pool_t*);

odroute_t*
od_routepool_new(odroute_pool_t*, odscheme_route_t*, char*, int, char*, int);

void od_routepool_unlink(odroute_pool_t*, odroute_t*);

odroute_t*
od_routepool_match(odroute_pool_t*, char*, int, char*, int);

#endif
