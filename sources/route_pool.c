
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
#include <ctype.h>
#include <inttypes.h>
#include <assert.h>

#include <machinarium.h>
#include <kiwi.h>
#include <odyssey.h>

void
od_route_pool_init(od_route_pool_t *pool)
{
	od_list_init(&pool->list);
	pool->count = 0;
}

void
od_route_pool_free(od_route_pool_t *pool)
{
	od_list_t *i, *n;
	od_list_foreach_safe(&pool->list, i, n) {
		od_route_t *route;
		route = od_container_of(i, od_route_t, link);
		od_route_free(route);
	}
}

static inline void
od_route_pool_gc_route(od_route_pool_t *pool, od_route_t *route)
{
	if (od_server_pool_total(&route->server_pool) > 0 ||
	    od_client_pool_total(&route->client_pool) > 0)
		return;

	/* gc dynamic or absolete routes */
	if (!od_route_is_dynamic(route) && !route->config->obsolete)
		return;

	od_config_route_unref(route->config);

	/* free route data */
	assert(pool->count > 0);
	pool->count--;
	od_list_unlink(&route->link);
	od_route_free(route);
}

void
od_route_pool_gc(od_route_pool_t *pool)
{
	od_list_t *i, *n;
	od_list_foreach_safe(&pool->list, i, n) {
		od_route_t *route;
		route = od_container_of(i, od_route_t, link);
		od_route_pool_gc_route(pool, route);
	}
}

od_route_t*
od_route_pool_new(od_route_pool_t *pool, od_config_route_t *config,
                  od_route_id_t *id)
{
	od_route_t *route = od_route_allocate();
	if (route == NULL)
		return NULL;
	int rc;
	rc = od_route_id_copy(&route->id, id);
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
od_route_pool_match(od_route_pool_t *pool,
                    od_route_id_t *key,
                    od_config_route_t *config)
{
	od_list_t *i;
	od_list_foreach(&pool->list, i) {
		od_route_t *route;
		route = od_container_of(i, od_route_t, link);
		if (route->config == config && od_route_id_compare(&route->id, key))
			return route;
	}
	return NULL;
}

od_server_t*
od_route_pool_next(od_route_pool_t *pool, od_server_state_t state)
{
	od_route_t *route;
	od_list_t *i, *n;
	od_list_foreach_safe(&pool->list, i, n) {
		route = od_container_of(i, od_route_t, link);
		od_server_t *server;
		server = od_server_pool_next(&route->server_pool, state);
		if (server)
			return server;
	}
	return NULL;
}

od_server_t*
od_route_pool_server_foreach(od_route_pool_t *pool, od_server_state_t state,
                             od_server_pool_cb_t callback,
                             void *arg)
{
	od_route_t *route;
	od_list_t *i, *n;
	od_list_foreach_safe(&pool->list, i, n) {
		route = od_container_of(i, od_route_t, link);
		od_server_t *server;
		server = od_server_pool_foreach(&route->server_pool, state,
		                                callback, arg);
		if (server)
			return server;
	}
	return NULL;
}

od_client_t*
od_route_pool_client_foreach(od_route_pool_t *pool, od_client_state_t state,
                             od_client_pool_cb_t callback,
                             void *arg)
{
	od_route_t *route;
	od_list_t *i, *n;
	od_list_foreach_safe(&pool->list, i, n) {
		route = od_container_of(i, od_route_t, link);
		od_client_t *client;
		client = od_client_pool_foreach(&route->client_pool, state,
		                                callback, arg);
		if (client)
			return client;
	}
	return NULL;
}

static inline void
od_route_pool_stat_database_mark(od_route_pool_t *pool,
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

		od_stat_sum(current, &route->stats);
		od_stat_sum(prev, &route->stats_prev);

		route->stats_mark++;
	}
}

static inline void
od_route_pool_stat_unmark(od_route_pool_t *pool)
{
	od_route_t *route;
	od_list_t *i;
	od_list_foreach(&pool->list, i) {
		route = od_container_of(i, od_route_t, link);
		route->stats_mark = 0;
	}
}

int
od_route_pool_stat_database(od_route_pool_t *pool,
                            od_route_pool_stat_database_cb_t callback,
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
		od_route_pool_stat_database_mark(pool,
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
			od_route_pool_stat_unmark(pool);
			return -1;
		}
	}

	od_route_pool_stat_unmark(pool);
	return 0;
}

int
od_route_pool_stat(od_route_pool_t *pool,
                   od_route_pool_stat_cb_t callback,
                   uint64_t prev_time_us,
                   void *arg)
{
	od_list_t *i;
	od_list_foreach(&pool->list, i)
	{
		od_route_t *route;
		route = od_container_of(i, od_route_t, link);

		od_stat_t current;
		od_stat_init(&current);
		od_stat_copy(&current, &route->stats);

		/* calculate average */
		od_stat_t avg;
		od_stat_init(&avg);
		od_stat_average(&avg, &current, &route->stats_prev, prev_time_us);

		int rc;
		rc = callback(route, &current, &avg, arg);
		if (rc == -1)
			return -1;
	}

	return 0;
}
