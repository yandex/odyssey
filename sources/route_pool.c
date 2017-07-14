
/*
 * Odissey.
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
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/syslog.h"
#include "sources/log.h"
#include "sources/scheme.h"
#include "sources/scheme_mgr.h"
#include "sources/config.h"
#include "sources/system.h"
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

void od_routepool_gc(od_routepool_t *pool)
{
	od_list_t *i, *n;
	od_list_foreach_safe(&pool->list, i, n) {
		od_route_t *route;
		route = od_container_of(i, od_route_t, link);
		od_routepool_gc_route(pool, route);
	}
}

static inline void
od_routepool_unlink(od_routepool_t *pool, od_route_t *route)
{
	assert(pool->count > 0);
	pool->count--;
	od_list_unlink(&route->link);
	od_route_free(route);
}

void od_routepool_gc_route(od_routepool_t *pool, od_route_t *route)
{
	if (od_serverpool_total(&route->server_pool) == 0 &&
	    od_clientpool_total(&route->client_pool) == 0)
	{
		od_routepool_unlink(pool, route);
	}
}

od_route_t*
od_routepool_new(od_routepool_t *pool, od_schemeuser_t *scheme,
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
	route->scheme = scheme;
	od_list_append(&pool->list, &route->link);
	pool->count++;
	return route;
}

od_route_t*
od_routepool_match(od_routepool_t *pool, od_routeid_t *key)
{
	od_route_t *route;
	od_list_t *i;
	od_list_foreach(&pool->list, i) {
		route = od_container_of(i, od_route_t, link);
		if (od_routeid_compare(&route->id, key))
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
od_routepool_foreach(od_routepool_t *pool, od_serverstate_t state,
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
