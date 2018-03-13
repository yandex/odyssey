
/*
 * Odyssey.
 *
 * Advanced PostgreSQL connection pooler.
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
#include "sources/config_mgr.h"
#include "sources/config_reader.h"
#include "sources/global.h"
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
	if (od_serverpool_total(&route->server_pool) > 0 ||
	    od_clientpool_total(&route->client_pool) > 0)
		return;

	od_configroute_t *config = route->config;

	/* free route data */
	assert(pool->count > 0);
	pool->count--;
	od_list_unlink(&route->link);
	od_route_free(route);

	/* maybe free obsolete config db */
	od_configroute_unref(config);

	if (config->is_obsolete && config->refs == 0)
		od_configroute_free(config);
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
od_routepool_stats_mark(od_routepool_t *pool,
                        char *database,
                        int   database_len,
                        od_serverstat_t *total,
                        od_serverstat_t *avg)
{
	int match = 0;
	od_list_t *i;
	od_list_foreach(&pool->list, i) {
		od_route_t *route;
		route = od_container_of(i, od_route_t, link);
		if (route->stats_mark)
			continue;
		if (route->id.database_len != database_len)
			continue;
		if (memcmp(route->id.database, database, database_len) != 0)
			continue;

		total->count_request += route->cron_stats.count_request;
		total->query_time    += route->cron_stats.query_time;
		total->recv_client   += route->cron_stats.recv_client;
		total->recv_server   += route->cron_stats.recv_server;

		avg->count_request   += route->cron_stats_avg.count_request;
		avg->query_time      += route->cron_stats_avg.query_time;
		avg->recv_client     += route->cron_stats_avg.recv_client;
		avg->recv_server     += route->cron_stats_avg.recv_server;

		route->stats_mark++;
		match++;
	}
	return match;
}

static inline void
od_routepool_stats_unmark(od_routepool_t *pool)
{
	od_route_t *route;
	od_list_t *i, *n;
	od_list_foreach_safe(&pool->list, i, n) {
		route = od_container_of(i, od_route_t, link);
		route->stats_mark = 0;
	}
}

int
od_routepool_stats(od_routepool_t *pool,
                   od_routepool_stats_function_t callback, void *arg)
{

	od_route_t *route;
	od_list_t *i, *n;
	od_list_foreach_safe(&pool->list, i, n) {
		route = od_container_of(i, od_route_t, link);
		if (route->stats_mark)
			continue;
		od_serverstat_t total;
		od_serverstat_t avg;
		memset(&total, 0, sizeof(total));
		memset(&avg, 0, sizeof(avg));
		int match;
		match = od_routepool_stats_mark(pool,
		                                route->id.database,
		                                route->id.database_len,
		                                &total, &avg);
		assert(match > 0);
		avg.count_request /= match;
		avg.query_time /= match;
		avg.recv_client /= match;
		avg.recv_server /= match;
		int rc;
		rc = callback(route->id.database, route->id.database_len, &total, &avg, arg);
		if (rc == -1) {
			od_routepool_stats_unmark(pool);
			return -1;
		}
	}
	od_routepool_stats_unmark(pool);
	return 0;
}
