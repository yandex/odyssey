
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
#include "od_client.h"
#include "od_client_pool.h"
#include "od.h"
#include "od_io.h"
#include "od_pooler.h"
#include "od_periodic.h"
#include "od_fe.h"
#include "od_be.h"

static inline int
od_expire_mark(od_server_t *server, void *arg)
{
	od_route_t *route = server->route;
	if (! mm_is_connected(server->io)) {
		od_serverpool_set(&route->server_pool, server,
		                  OD_SCLOSE);
		return 0;
	}
	if (! route->scheme->ttl)
		return 0;
	if (server->idle_time < route->scheme->ttl) {
		server->idle_time++;
		return 0;
	}
	od_serverpool_set(&route->server_pool, server,
	                  OD_SEXPIRE);
	return 0;
}

void od_periodic(void *arg)
{
	od_pooler_t *pooler = arg;

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
		od_routepool_foreach(&pooler->route_pool, OD_SIDLE,
		                     od_expire_mark,
		                     pooler);

		/* sweep disconnected servers */
		for (;;) {
			od_server_t *server =
				od_routepool_next(&pooler->route_pool, OD_SCLOSE);
			if (server == NULL)
				break;
			od_debug(&pooler->od->log, "S: closed connection",
			         server->idle_time);
			server->idle_time = 0;
			od_beclose(server);
		}

		/* sweep expired connections */
		for (;;) {
			od_server_t *server =
				od_routepool_next(&pooler->route_pool, OD_SEXPIRE);
			if (server == NULL)
				break;
			od_debug(&pooler->od->log, "S: closing idle connection (%d secs)",
			         server->idle_time);
			server->idle_time = 0;
			od_beterminate(server);
			od_beclose(server);
		}

		/* 1 second soft interval */
		mm_sleep(pooler->env, 1000);
	}
}
