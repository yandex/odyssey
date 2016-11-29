#ifndef OD_ROUTE_POOL_H_
#define OD_ROUTE_POOL_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct odroute_pool_t odroute_pool_t;

struct odroute_pool_t {
	od_list_t list;
	int       count;
};

void od_routepool_init(odroute_pool_t*);
void od_routepool_free(odroute_pool_t*);

od_route_t*
od_routepool_new(odroute_pool_t*, od_schemeroute_t*,
                 od_routeid_t*);

void od_routepool_unlink(odroute_pool_t*, od_route_t*);

od_route_t*
od_routepool_match(odroute_pool_t*, od_routeid_t*);

od_server_t*
od_routepool_pop(odroute_pool_t*, od_serverstate_t);

od_server_t*
od_routepool_foreach(odroute_pool_t*, od_serverstate_t,
                     od_serverpool_cb_t, void*);

#endif
