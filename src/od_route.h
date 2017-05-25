#ifndef OD_ROUTE_H
#define OD_ROUTE_H

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_route od_route_t;

struct od_route
{
	od_schemeroute_t *scheme;
	od_routeid_t      id;
	od_serverpool_t   server_pool;
	od_clientpool_t   client_pool;
	od_list_t         link;
};

static inline void
od_route_init(od_route_t *route)
{
	route->scheme = NULL;
	od_routeid_init(&route->id);
	od_serverpool_init(&route->server_pool);
	od_clientpool_init(&route->client_pool);
	od_list_init(&route->link);
}

static inline od_route_t*
od_route_allocate(void) {
	od_route_t *route = malloc(sizeof(*route));
	if (route == NULL)
		return NULL;
	od_route_init(route);
	return route;
}

static inline void
od_route_free(od_route_t *route)
{
	od_routeid_free(&route->id);
	od_serverpool_free(&route->server_pool);
	free(route);
}

#endif /* OD_ROUTE_H */
