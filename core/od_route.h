#ifndef OD_ROUTE_H_
#define OD_ROUTE_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_route_t od_route_t;

struct od_route_t {
	od_schemeroute_t *scheme;
	od_routeid_t      id;
	od_serverpool_t   server_pool;
	od_clientpool_t   client_pool;
	int               client_count;
	od_list_t         link;
};

static inline void
od_routeinit(od_route_t *route)
{
	route->scheme = NULL;
	route->client_count = 0;
	od_routeid_init(&route->id);
	od_serverpool_init(&route->server_pool);
	od_clientpool_init(&route->client_pool);
	od_listinit(&route->link);
}

static inline od_route_t*
od_routealloc(void) {
	od_route_t *route = malloc(sizeof(*route));
	if (route == NULL)
		return NULL;
	od_routeinit(route);
	return route;
}

static inline void
od_routefree(od_route_t *route)
{
	od_routeid_free(&route->id);
	od_serverpool_free(&route->server_pool);
	free(route);
}

#endif
