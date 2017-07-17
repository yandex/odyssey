#ifndef OD_ROUTE_POOL_H
#define OD_ROUTE_POOL_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_routepool od_routepool_t;

struct od_routepool
{
	od_list_t list;
	int count;
};

void od_routepool_init(od_routepool_t*);
void od_routepool_free(od_routepool_t*);
void od_routepool_gc(od_routepool_t*, od_route_t*);

od_route_t*
od_routepool_new(od_routepool_t*, od_schemeuser_t*,
                 od_routeid_t*);

od_route_t*
od_routepool_match(od_routepool_t*, od_routeid_t*);

od_server_t*
od_routepool_next(od_routepool_t*, od_serverstate_t);

od_server_t*
od_routepool_foreach(od_routepool_t*, od_serverstate_t,
                     od_serverpool_cb_t, void*);

#endif /* OD_ROUTE_POOL_H */
