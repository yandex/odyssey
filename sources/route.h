#ifndef OD_ROUTE_H
#define OD_ROUTE_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_route od_route_t;

struct od_route
{
	od_configroute_t *config;
	od_routeid_t      id;
	od_serverstat_t   cron_stats;
	od_serverstat_t   cron_stats_avg;
	int               stats_mark;
	od_serverpool_t   server_pool;
	od_clientpool_t   client_pool;
	od_list_t         link;
};

static inline void
od_route_init(od_route_t *route)
{
	route->config = NULL;
	od_routeid_init(&route->id);
	od_serverpool_init(&route->server_pool);
	od_clientpool_init(&route->client_pool);
	route->stats_mark = 0;
	memset(&route->cron_stats, 0, sizeof(route->cron_stats));
	memset(&route->cron_stats_avg, 0, sizeof(route->cron_stats_avg));
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
	od_routeid_free(&route->id);
	od_serverpool_free(&route->server_pool);
	free(route);
}

static inline int
od_route_is_dynamic(od_route_t *route)
{
	return route->config->db_is_default || route->config->user_is_default;
}

#endif /* OD_ROUTE_H */
