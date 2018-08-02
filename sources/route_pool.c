
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include <machinarium.h>
#include <shapito.h>

#include "sources/macro.h"
#include "sources/atomic.h"
#include "sources/util.h"
#include "sources/error.h"
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/logger.h"
#include "sources/config.h"
#include "sources/config_reader.h"
#include "sources/global.h"
#include "sources/stat.h"
#include "sources/server.h"
#include "sources/server_pool.h"
#include "sources/client.h"
#include "sources/client_pool.h"
#include "sources/route_id.h"
#include "sources/route.h"
#include "sources/route_pool.h"

void od_routepool_init(od_routepool_t *pool)
{
	od_list_init(&pool->list);
	pool->count = 0;
}

void od_routepool_free(od_routepool_t *pool)
{
	od_list_t *i, *n;
	od_list_foreach_safe(&pool->list, i, n) {
		od_route_t *route;
		route = od_container_of(i, od_route_t, link);
		od_route_free(route);
	}
}

static inline void
od_routepool_gc_route(od_routepool_t *pool, od_route_t *route)
{
	/* skip static routes */
	if (! od_route_is_dynamic(route))
		return;

	if (od_serverpool_total(&route->server_pool) > 0 ||
	    od_clientpool_total(&route->client_pool) > 0)
		return;

	/* free route data */
	assert(pool->count > 0);
	pool->count--;
	od_list_unlink(&route->link);
	od_route_free(route);
}

void od_routepool_gc(od_routepool_t *pool)
{
	od_list_t *i, *n;
	od_list_foreach_safe(&pool->list, i, n) {
		od_route_t *route;
		route = od_container_of(i, od_route_t, link);
		od_routepool_gc_route(pool, route);
	}
}

od_route_t*
od_routepool_new(od_routepool_t *pool, od_configroute_t *config,
                 od_routeid_t *id)
{
	od_route_t *route = od_route_allocate();
	if (route == NULL)
		return NULL;
	int rc;
	rc = od_routeid_copy(&route->id, id);
	if (rc == -1) {
		od_route_free(route);
		return NULL;
	}
	route->config = config;
	od_list_append(&pool->list, &route->link);
	pool->count++;
	return route;
}

od_route_t*
od_routepool_match(od_routepool_t *pool,
                   od_routeid_t *key,
                   od_configroute_t *config)
{
	od_list_t *i;
	od_list_foreach(&pool->list, i) {
		od_route_t *route;
		route = od_container_of(i, od_route_t, link);
		if (route->config == config && od_routeid_compare(&route->id, key))
			return route;
	}
	return NULL;
}

od_server_t*
od_routepool_next(od_routepool_t *pool, od_serverstate_t state)
{
	od_route_t *route;
	od_list_t *i, *n;
	od_list_foreach_safe(&pool->list, i, n) {
		route = od_container_of(i, od_route_t, link);
		od_server_t *server;
		server = od_serverpool_next(&route->server_pool, state);
		if (server)
			return server;
	}
	return NULL;
}

od_server_t*
od_routepool_server_foreach(od_routepool_t *pool, od_serverstate_t state,
                            od_serverpool_cb_t callback,
                            void *arg)
{
	od_route_t *route;
	od_list_t *i, *n;
	od_list_foreach_safe(&pool->list, i, n) {
		route = od_container_of(i, od_route_t, link);
		od_server_t *server;
		server = od_serverpool_foreach(&route->server_pool, state,
		                               callback, arg);
		if (server)
			return server;
	}
	return NULL;
}

od_client_t*
od_routepool_client_foreach(od_routepool_t *pool, od_clientstate_t state,
                            od_clientpool_cb_t callback,
                            void *arg)
{
	od_route_t *route;
	od_list_t *i, *n;
	od_list_foreach_safe(&pool->list, i, n) {
		route = od_container_of(i, od_route_t, link);
		od_client_t *client;
		client = od_clientpool_foreach(&route->client_pool, state,
		                               callback, arg);
		if (client)
			return client;
	}
	return NULL;
}

static inline int
od_routepool_stat_server(od_server_t *server, void *arg)
{
	od_stat_t *stats = arg;
	od_stat_sum(stats, &server->stats);
	return 0;
}

static inline void
od_routepool_stat_database_mark(od_routepool_t *pool,
                                char *database,
                                int   database_len,
                                od_stat_t *current,
                                od_stat_t *prev)
{
	od_list_t *i;
	od_list_foreach(&pool->list, i)
	{
		od_route_t *route;
		route = od_container_of(i, od_route_t, link);
		if (route->stats_mark)
			continue;
		if (route->id.database_len != database_len)
			continue;
		if (memcmp(route->id.database, database, database_len) != 0)
			continue;

		/* sum actual stats per server */
		od_serverpool_foreach(&route->server_pool, OD_SACTIVE,
		                      od_routepool_stat_server,
		                      current);

		od_serverpool_foreach(&route->server_pool, OD_SIDLE,
		                      od_routepool_stat_server,
		                      current);

		/* sum previous cron stats per route */
		od_stat_sum(prev, &route->stats_cron);

		route->stats_mark++;
	}
}

static inline void
od_routepool_stat_unmark(od_routepool_t *pool)
{
	od_route_t *route;
	od_list_t *i;
	od_list_foreach(&pool->list, i) {
		route = od_container_of(i, od_route_t, link);
		route->stats_mark = 0;
	}
}

int
od_routepool_stat_database(od_routepool_t *pool,
                           od_routepool_stat_database_cb_t callback,
                           uint64_t prev_time_us,
                           void *arg)
{
	od_route_t *route;
	od_list_t *i;
	od_list_foreach(&pool->list, i)
	{
		route = od_container_of(i, od_route_t, link);
		if (route->stats_mark)
			continue;

		/* gather current and previous cron stats */
		od_stat_t current;
		od_stat_t prev;
		od_stat_init(&current);
		od_stat_init(&prev);
		od_routepool_stat_database_mark(pool,
		                                route->id.database,
		                                route->id.database_len,
		                                &current, &prev);

		/* calculate average */
		od_stat_t avg;
		od_stat_init(&avg);
		od_stat_average(&avg, &current, &prev, prev_time_us);

		int rc;
		rc = callback(route->id.database, route->id.database_len - 1,
		              &current, &avg, arg);
		if (rc == -1) {
			od_routepool_stat_unmark(pool);
			return -1;
		}
	}

	od_routepool_stat_unmark(pool);
	return 0;
}

int
od_routepool_stat(od_routepool_t *pool,
                  od_routepool_stat_cb_t callback,
                  uint64_t prev_time_us,
                  void *arg)
{
	od_list_t *i;
	od_list_foreach(&pool->list, i)
	{
		od_route_t *route;
		route = od_container_of(i, od_route_t, link);

		/* gather current statistics per route server pool */
		od_stat_t current;
		od_stat_init(&current);

		od_serverpool_foreach(&route->server_pool, OD_SACTIVE,
		                      od_routepool_stat_server,
		                      &current);

		od_serverpool_foreach(&route->server_pool, OD_SIDLE,
		                      od_routepool_stat_server,
		                      &current);

		/* calculate average */
		od_stat_t avg;
		od_stat_init(&avg);
		od_stat_average(&avg, &current, &route->stats_cron, prev_time_us);

		int rc;
		rc = callback(route, &current, &avg, arg);
		if (rc == -1)
			return -1;
	}

	return 0;
}
