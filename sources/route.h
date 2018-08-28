#ifndef ODYSSEY_ROUTE_H
#define ODYSSEY_ROUTE_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_route od_route_t;

struct od_route
{
	od_config_route_t *config;
	od_route_id_t      id;
	od_stat_t          stats;
	od_stat_t          stats_prev;
	int                stats_mark;
	od_server_pool_t   server_pool;
	od_client_pool_t   client_pool;
	od_list_t          link;
};

static inline void
od_route_init(od_route_t *route)
{
	route->config = NULL;
	od_route_id_init(&route->id);
	od_server_pool_init(&route->server_pool);
	od_client_pool_init(&route->client_pool);
	route->stats_mark = 0;
	od_stat_init(&route->stats);
	od_stat_init(&route->stats_prev);
	od_list_init(&route->link);
}

static inline od_route_t*
od_route_allocate(void)
{
	od_route_t *route = malloc(sizeof(*route));
	if (route == NULL)
		return NULL;
	od_route_init(route);
	return route;
}

static inline void
od_route_free(od_route_t *route)
{
	od_route_id_free(&route->id);
	od_server_pool_free(&route->server_pool);
	free(route);
}

static inline int
od_route_is_dynamic(od_route_t *route)
{
	return route->config->db_is_default || route->config->user_is_default;
}

#endif /* ODYSSEY_ROUTE_H */
