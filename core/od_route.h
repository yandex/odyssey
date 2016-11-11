#ifndef OD_ROUTE_H_
#define OD_ROUTE_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct odroute_t odroute_t;

struct odroute_t {
	odscheme_route_t *scheme;
	odserver_pool_t   server_pool;
	char             *user;
	int               user_len;
	char             *database;
	int               database_len;
	odlist_t          link;
};

static inline void
od_routeinit(odroute_t *route)
{
	route->scheme       = NULL;
	route->user         = NULL;
	route->user_len     = 0;
	route->database     = NULL;
	route->database_len = 0;
	od_serverpool_init(&route->server_pool);
	od_listinit(&route->link);
}

static inline odroute_t*
od_routealloc(void) {
	odroute_t *route = malloc(sizeof(*route));
	if (route == NULL)
		return NULL;
	od_routeinit(route);
	return route;
}

static inline void
od_routefree(odroute_t *route)
{
	if (route->database)
		free(route->database);
	if (route->user)
		free(route->user);
	od_serverpool_free(&route->server_pool);
	free(route);
}

#endif
