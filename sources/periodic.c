
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
#include <signal.h>

#include <machinarium.h>
#include <shapito.h>

#include "sources/macro.h"
#include "sources/version.h"
#include "sources/atomic.h"
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/log_file.h"
#include "sources/log_system.h"
#include "sources/logger.h"
#include "sources/daemon.h"
#include "sources/scheme.h"
#include "sources/scheme_mgr.h"
#include "sources/config.h"
#include "sources/msg.h"
#include "sources/system.h"
#include "sources/instance.h"
#include "sources/server.h"
#include "sources/server_pool.h"
#include "sources/client.h"
#include "sources/client_pool.h"
#include "sources/route_id.h"
#include "sources/route.h"
#include "sources/route_pool.h"
#include "sources/io.h"
#include "sources/router.h"
#include "sources/pooler.h"
#include "sources/relay.h"
#include "sources/frontend.h"
#include "sources/backend.h"
#include "sources/periodic.h"

static inline int
od_periodic_stats_server(od_server_t *server, void *arg)
{
	od_serverstat_t *stats = arg;
	stats->query_time    += od_atomic_u64_of(&server->stats.query_time);
	stats->count_reply   += od_atomic_u64_of(&server->stats.count_reply);
	stats->count_request += od_atomic_u64_of(&server->stats.count_request);
	return 0;
}

static inline void
od_periodic_stats(od_router_t *router)
{
	od_instance_t *instance = router->system->instance;
	if (router->route_pool.count == 0)
		return;
	od_log(&instance->logger, "statistics");
	od_list_t *i;
	od_list_foreach(&router->route_pool.list, i)
	{
		od_route_t *route;
		route = od_container_of(i, od_route_t, link);

		/* gather statistics per route server pool */
		od_serverstat_t stats;
		memset(&stats, 0, sizeof(stats));
		od_serverpool_foreach(&route->server_pool, OD_SACTIVE,
		                      od_periodic_stats_server,
		                      &stats);
		od_serverpool_foreach(&route->server_pool, OD_SIDLE,
		                      od_periodic_stats_server,
		                      &stats);

		uint64_t count_request = 0;
		float query_time = 0;
		if (stats.count_request >= route->periodic_stats.count_request) {
			count_request = (stats.count_request - route->periodic_stats.count_request) /
			                 instance->scheme.log_statistics;
			if (count_request > 0) {
				query_time = (float)(stats.query_time - route->periodic_stats.query_time) /
				              count_request;
			}
		}

		/* update stats */
		route->periodic_stats = stats;

		od_log(&instance->logger,
		       "  [%.*s.%.*s.%" PRIu64 "] %sclients %d, "
		       "pool_active %d, "
		       "pool_idle %d "
		       "requests %" PRIu64 " "
		       "query_time %.2f",
		       route->id.database_len,
		       route->id.database,
		       route->id.user_len,
		       route->id.user,
		       route->scheme->version,
		       route->scheme->is_obsolete ? "(obsolete) " : "",
		       od_clientpool_total(&route->client_pool),
		       route->server_pool.count_active,
		       route->server_pool.count_idle,
		       count_request,
		       query_time);
	}
}

static inline int
od_periodic_expire_mark(od_server_t *server, void *arg)
{
	od_router_t *router = arg;
	od_instance_t *instance = router->system->instance;
	od_route_t *route = server->route;

	/* expire by server scheme obsoletion */
	if (route->scheme->is_obsolete &&
	    od_clientpool_total(&route->client_pool) == 0) {
		od_debug_server(&instance->logger, &server->id, "expire",
		                "scheme marked as obsolete, schedule closing");
		od_serverpool_set(&route->server_pool, server,
		                  OD_SEXPIRE);
		return 0;
	}

	/* expire by time-to-live */
	if (! route->scheme->pool_ttl)
		return 0;

	od_debug_server(&instance->logger, &server->id, "expire",
	                "idle time: %d",
	                server->idle_time);
	if (server->idle_time < route->scheme->pool_ttl) {
		server->idle_time++;
		return 0;
	}
	od_serverpool_set(&route->server_pool, server,
	                  OD_SEXPIRE);
	return 0;
}

static inline void
od_periodic_expire(od_periodic_t *periodic)
{
	od_router_t *router = periodic->system->router;
	od_instance_t *instance = periodic->system->instance;

	/* Idle servers expire.
	 *
	 * It is important that mark logic stage must not yield
	 * to maintain iterator consistency.
	 *
	 * mark:
	 *
	 *  - If a server idle time is equal to ttl, then move
	 *    it to the EXPIRE queue.
	 *
	 *  - If a server scheme marked as obsolete and route has
	 *    no remaining clients, then move it to the EXPIRE queue.
	 *
	 *  - Add plus one idle second on each traversal.
	 *
	 * sweep:
	 *
	 *  - Foreach servers in EXPIRE queue, send Terminate
	 *    and close the connection.
	*/

	/* mark */
	od_routepool_foreach(&router->route_pool, OD_SIDLE,
	                     od_periodic_expire_mark,
	                     router);

	/* sweep */
	for (;;) {
		od_server_t *server;
		server = od_routepool_next(&router->route_pool, OD_SEXPIRE);
		if (server == NULL)
			break;
		od_debug_server(&instance->logger, &server->id, "expire",
		                "closing idle connection (%d secs)",
		                server->idle_time);
		server->idle_time = 0;

		od_route_t *route = server->route;
		server->route = NULL;
		od_serverpool_set(&route->server_pool, server, OD_SUNDEF);

		machine_io_attach(server->io);

		od_backend_terminate(server);
		od_backend_close(server);
	}

	/* cleanup unused dynamic routes and obsolete
	 * db schemes */
	od_routepool_gc(&router->route_pool);
}

static void
od_periodic(void *arg)
{
	od_periodic_t *periodic = arg;
	od_router_t *router = periodic->system->router;
	od_instance_t *instance = periodic->system->instance;

	int tick = 0;
	for (;;)
	{
		/* mark and sweep expired idle server connections */
		od_periodic_expire(periodic);

		/* stats */
		if (instance->scheme.log_statistics > 0) {
			tick++;
			if (tick >= instance->scheme.log_statistics) {
				od_periodic_stats(router);
				tick = 0;
			}
		}

		/* 1 second soft interval */
		machine_sleep(1000);
	}
}

int od_periodic_init(od_periodic_t *periodic, od_system_t *system)
{
	periodic->system = system;
	return 0;
}

int od_periodic_start(od_periodic_t *periodic)
{
	od_instance_t *instance = periodic->system->instance;
	int64_t coroutine_id;
	coroutine_id = machine_coroutine_create(od_periodic, periodic);
	if (coroutine_id == -1) {
		od_error(&instance->logger, "periodic", "failed to start periodic coroutine");
		return -1;
	}
	return 0;
}
