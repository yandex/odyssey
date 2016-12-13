
/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <machinarium.h>
#include <soprano.h>

#include "od_macro.h"
#include "od_list.h"
#include "od_pid.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"
#include "od_server.h"
#include "od_server_pool.h"
#include "od_route_id.h"
#include "od_route.h"
#include "od_route_pool.h"

void od_routepool_init(od_routepool_t *pool)
{
	od_listinit(&pool->list);
	pool->count = 0;
}

void od_routepool_free(od_routepool_t *pool)
{
	od_route_t *route;
	od_list_t *i, *n;
	od_listforeach_safe(&pool->list, i, n) {
		route = od_container_of(i, od_route_t, link);
		od_routefree(route);
	}
}

od_route_t*
od_routepool_new(od_routepool_t *pool, od_schemeroute_t *scheme,
                 od_routeid_t *id)
{
	od_route_t *route = od_routealloc();
	if (route == NULL)
		return NULL;
	int rc;
	rc = od_routeid_copy(&route->id, id);
	if (rc == -1) {
		od_routefree(route);
		return NULL;
	}
	route->scheme = scheme;
	od_listappend(&pool->list, &route->link);
	pool->count++;
	return route;
}

void od_routepool_unlink(od_routepool_t *pool, od_route_t *route)
{
	assert(pool->count > 0);
	pool->count--;
	od_listunlink(&route->link);
	od_routefree(route);
}

od_route_t*
od_routepool_match(od_routepool_t *pool, od_routeid_t *key)
{
	od_route_t *route;
	od_list_t *i;
	od_listforeach(&pool->list, i) {
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
	od_listforeach_safe(&pool->list, i, n) {
		route = od_container_of(i, od_route_t, link);
		od_server_t *server =
			od_serverpool_next(&route->server_pool, state);
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
	od_listforeach_safe(&pool->list, i, n) {
		route = od_container_of(i, od_route_t, link);
		od_server_t *server =
			od_serverpool_foreach(&route->server_pool, state,
			                      callback, arg);
		if (server)
			return server;
	}
	return NULL;
}
