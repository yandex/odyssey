
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
#include "sources/util.h"
#include "sources/error.h"
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/logger.h"
#include "sources/daemon.h"
#include "sources/scheme.h"
#include "sources/scheme_mgr.h"
#include "sources/config.h"
#include "sources/msg.h"
#include "sources/system.h"
#include "sources/server.h"
#include "sources/server_pool.h"
#include "sources/client.h"
#include "sources/client_pool.h"
#include "sources/route_id.h"
#include "sources/route.h"
#include "sources/route_pool.h"
#include "sources/io.h"
#include "sources/instance.h"
#include "sources/router_cancel.h"
#include "sources/router.h"
#include "sources/pooler.h"
#include "sources/worker.h"
#include "sources/frontend.h"
#include "sources/backend.h"
#include "sources/cron.h"

static inline int
od_cron_stats_server(od_server_t *server, void *arg)
{
	od_serverstat_t *stats = arg;
	stats->query_time    += od_atomic_u64_of(&server->stats.query_time);
	stats->count_request += od_atomic_u64_of(&server->stats.count_request);
	stats->recv_client   += od_atomic_u64_of(&server->stats.recv_client);
	stats->recv_server   += od_atomic_u64_of(&server->stats.recv_server);
	return 0;
}

static inline void
od_cron_stats(od_router_t *router)
{
	od_instance_t *instance = router->system->instance;

	if (router->route_pool.count == 0)
		return;

	if (instance->scheme.log_stats)
	{
		int stream_count = 0;
		int stream_count_allocated = 0;
		int stream_total_allocated = 0;
		int stream_cache_size = 0;
		shapito_cache_stat(&instance->stream_cache, &stream_count,
		                   &stream_count_allocated, &stream_total_allocated,
		                   &stream_cache_size);
		int count_machine = 0;
		int count_coroutine = 0;
		int count_coroutine_cache = 0;
		machinarium_stat(&count_machine, &count_coroutine,
		                 &count_coroutine_cache);
		od_log(&instance->logger, "stats", NULL, NULL,
		       "clients %d, stream cache (%d:%d allocated, %d cached %d bytes), "
		       "coroutines (%d active, %d cached)",
		       router->clients,
		       stream_count_allocated,
		       stream_total_allocated,
		       stream_count,
		       stream_cache_size,
		       count_coroutine,
		       count_coroutine_cache);
	}

	od_list_t *i;
	od_list_foreach(&router->route_pool.list, i)
	{
		od_route_t *route;
		route = od_container_of(i, od_route_t, link);

		/* gather statistics per route server pool */
		od_serverstat_t stats;
		memset(&stats, 0, sizeof(stats));

		od_serverpool_foreach(&route->server_pool, OD_SACTIVE,
		                      od_cron_stats_server,
		                      &stats);
		od_serverpool_foreach(&route->server_pool, OD_SIDLE,
		                      od_cron_stats_server,
		                      &stats);

		/* calculate average between previous sample and the
		   current one */
		uint64_t recv_client = 0;
		uint64_t recv_server = 0;
		uint64_t reqs = 0;
		uint64_t query_time = 0;

		/* ensure server stats not changed due to a
		 * server connection close */
		int64_t  reqs_diff_sanity;
		reqs_diff_sanity = (stats.count_request - route->cron_stats.count_request);

		if (reqs_diff_sanity >= 0)
		{
			/* request count */
			uint64_t reqs_prev = 0;
			reqs_prev = route->cron_stats.count_request /
			            instance->scheme.stats_interval;

			uint64_t reqs_current = 0;
			reqs_current = stats.count_request /
			               instance->scheme.stats_interval;

			int64_t reqs_diff;
			reqs_diff = reqs_current - reqs_prev;

			reqs = reqs_diff / instance->scheme.stats_interval;

			/* recv client */
			uint64_t recv_client_prev = 0;
			recv_client_prev = route->cron_stats.recv_client /
			                    instance->scheme.stats_interval;

			uint64_t recv_client_current = 0;
			recv_client_current = stats.recv_client /
			                      instance->scheme.stats_interval;

			recv_client = (recv_client_current - recv_client_prev) /
			               instance->scheme.stats_interval;

			/* recv server */
			uint64_t recv_server_prev = 0;
			recv_server_prev = route->cron_stats.recv_server /
			                   instance->scheme.stats_interval;

			uint64_t recv_server_current = 0;
			recv_server_current = stats.recv_server /
			                      instance->scheme.stats_interval;

			recv_server = (recv_server_current - recv_server_prev) /
			               instance->scheme.stats_interval;

			/* query time */
			if (reqs_diff > 0)
				query_time = (stats.query_time - route->cron_stats.query_time) /
				              reqs_diff;
		}

		/* update stats */
		route->cron_stats = stats;

		route->cron_stats_avg.count_request = reqs;
		route->cron_stats_avg.recv_client   = recv_client;
		route->cron_stats_avg.recv_server   = recv_server;
		route->cron_stats_avg.query_time    = query_time;

		if (instance->scheme.log_stats) {
			od_log(&instance->logger, "stats", NULL, NULL,
			       "[%.*s.%.*s.%" PRIu64 "] %sclients %d, "
			       "pool_active %d, "
			       "pool_idle %d "
			       "rps %" PRIu64 " "
			       "query_time_us %" PRIu64 " "
			       "recv_client_bytes %" PRIu64 " "
			       "recv_server_bytes %" PRIu64,
			       route->id.database_len,
			       route->id.database,
			       route->id.user_len,
			       route->id.user,
			       route->scheme->version,
			       route->scheme->is_obsolete ? "(obsolete) " : "",
			       od_clientpool_total(&route->client_pool),
			       route->server_pool.count_active,
			       route->server_pool.count_idle,
			       reqs,
			       query_time,
			       recv_client,
			       recv_server);
		}
	}
}

static inline int
od_cron_expire_mark(od_server_t *server, void *arg)
{
	od_router_t *router = arg;
	od_instance_t *instance = router->system->instance;
	od_route_t *route = server->route;

	/* expire by server scheme obsoletion */
	if (route->scheme->is_obsolete &&
	    od_clientpool_total(&route->client_pool) == 0) {
		od_debug(&instance->logger, "expire", NULL, server,
		         "scheme marked as obsolete, schedule closing");
		od_serverpool_set(&route->server_pool, server,
		                  OD_SEXPIRE);
		return 0;
	}

	/* expire by time-to-live */
	if (! route->scheme->pool_ttl)
		return 0;

	od_debug(&instance->logger, "expire", NULL, server,
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
od_cron_expire(od_cron_t *cron)
{
	od_router_t *router = cron->system->router;
	od_instance_t *instance = cron->system->instance;

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
	od_routepool_server_foreach(&router->route_pool, OD_SIDLE,
	                            od_cron_expire_mark,
	                            router);

	/* sweep */
	for (;;) {
		od_server_t *server;
		server = od_routepool_next(&router->route_pool, OD_SEXPIRE);
		if (server == NULL)
			break;
		od_debug(&instance->logger, "expire", NULL, server,
		         "closing idle server connection (%d secs)",
		         server->idle_time);
		server->idle_time = 0;

		od_route_t *route = server->route;
		server->route = NULL;
		od_serverpool_set(&route->server_pool, server, OD_SUNDEF);

		if (instance->is_shared)
			machine_io_attach(server->io);

		od_backend_close_connection(server);
		od_backend_close(server);
	}

	/* cleanup unused dynamic routes and obsolete
	 * db schemes */
	od_routepool_gc(&router->route_pool);
}

static void
od_cron(void *arg)
{
	od_cron_t *cron = arg;
	od_router_t *router = cron->system->router;
	od_instance_t *instance = cron->system->instance;

	int stats_tick = 0;
	for (;;)
	{
		/* mark and sweep expired idle server connections */
		od_cron_expire(cron);

		/* update stats */
		if (++stats_tick >= instance->scheme.stats_interval) {
			od_cron_stats(router);
			stats_tick = 0;
		}

		/* 1 second soft interval */
		machine_sleep(1000);
	}
}

void od_cron_init(od_cron_t *cron, od_system_t *system)
{
	cron->system = system;
}

int od_cron_start(od_cron_t *cron)
{
	od_instance_t *instance = cron->system->instance;
	int64_t coroutine_id;
	coroutine_id = machine_coroutine_create(od_cron, cron);
	if (coroutine_id == -1) {
		od_error(&instance->logger, "cron", NULL, NULL,
		         "failed to start cron coroutine");
		return -1;
	}
	return 0;
}
