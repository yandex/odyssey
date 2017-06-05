
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
#include <inttypes.h>
#include <signal.h>

#include <machinarium.h>
#include <soprano.h>

#include "od_macro.h"
#include "od_version.h"
#include "od_list.h"
#include "od_pid.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_daemon.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"
#include "od_msg.h"
#include "od_system.h"
#include "od_instance.h"

#include "od_server.h"
#include "od_server_pool.h"
#include "od_client.h"
#include "od_client_pool.h"
#include "od_route_id.h"
#include "od_route.h"
#include "od_route_pool.h"
#include "od_io.h"

#include "od_router.h"
#include "od_pooler.h"
#include "od_relay.h"
#include "od_frontend.h"
#include "od_backend.h"
#include "od_periodic.h"

static inline void
od_periodic_stats (od_router_t *router)
{
	od_instance_t *instance = router->system->instance;
	if (router->route_pool.count == 0)
		return;
	od_log(&instance->log, "statistics");
	od_list_t *i;
	od_list_foreach(&router->route_pool.list, i) {
		od_route_t *route;
		route = od_container_of(i, od_route_t, link);
		od_log(&instance->log,
		       "  [%.*s, %.*s] clients %d, "
		       "pool_active %d, "
		       "pool_idle %d ",
		       route->id.database_len,
		       route->id.database,
		       route->id.user_len,
		       route->id.user,
			   od_clientpool_total(&route->client_pool),
		       route->server_pool.count_active,
		       route->server_pool.count_idle);
	}
}

static inline int
od_expire_mark(od_server_t *server, void *arg)
{
	od_router_t *router = arg;
	od_instance_t *instance = router->system->instance;

	od_route_t *route = server->route;
	/*
	if (! machine_connected(server->io)) {
		od_serverpool_set(&route->server_pool, server,
		                  OD_SCLOSE);
		return 0;
	}
	*/
	if (! route->scheme->ttl)
		return 0;
	od_debug_server(&instance->log, server->id, "expire",
	                "idle time: %d",
	                server->idle_time);
	if (server->idle_time < route->scheme->ttl) {
		server->idle_time++;
		return 0;
	}
	od_serverpool_set(&route->server_pool, server,
	                  OD_SEXPIRE);
	return 0;
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
		/* idle servers expire.
		 *
		 * 1. Add plus one idle second on each traversal.
		 *    If a server idle time is equal to ttl, then move
		 *    it to the EXPIRE queue.
		 *
		 *    It is important that this function must not yield.
		 *
		 * 2. Foreach servers in EXPIRE queue, send Terminate
		 *    and close the connection.
		*/

		/* mark servers for gc */
		od_routepool_foreach(&router->route_pool, OD_SIDLE,
		                     od_expire_mark,
		                     router);

#if 0
		/* sweep disconnected servers */
		for (;;) {
			od_server_t *server;
			server = od_routepool_next(&router->route_pool, OD_SCLOSE);
			if (server == NULL)
				break;
			od_log(&instance->log, server->io, "S: disconnected",
			       server->idle_time);
			server->idle_time = 0;
			/*
			od_beclose(server);
			*/
		}
#endif

		/* sweep expired connections */
		for (;;) {
			od_server_t *server;
			server = od_routepool_next(&router->route_pool, OD_SEXPIRE);
			if (server == NULL)
				break;
			od_debug_server(&instance->log, server->id, "expire",
			                "closing idle connection (%d secs)",
			                server->idle_time);
			server->idle_time = 0;

			od_route_t *route = server->route;
			server->route = NULL;
			od_serverpool_set(&route->server_pool, server, OD_SUNDEF);

			machine_io_attach(server->io);

			od_backend_terminate(server);
			od_backend_close(server);

			/* cleanup unused dynamic routes */
			od_routepool_gc(&router->route_pool);
		}

		/* stats */
		if (instance->scheme.stats_period > 0) {
			tick++;
			if (tick >= instance->scheme.stats_period) {
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
		od_error(&instance->log, "failed to start router");
		return 1;
	}
	return 0;
}
