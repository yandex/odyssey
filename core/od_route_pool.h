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
od_routepool_new(odroute_pool_t*, odscheme_route_t*,
                 odroute_id_t*);

void od_routepool_unlink(odroute_pool_t*, odroute_t*);

odroute_t*
od_routepool_match(odroute_pool_t*, odroute_id_t*);

odserver_t*
od_routepool_first(odroute_pool_t*, odserver_state_t);

odserver_t*
od_routepool_foreach(odroute_pool_t*, odserver_state_t,
                     odserver_pool_cb_t, void*);

#endif
