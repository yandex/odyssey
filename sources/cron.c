
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
#include "sources/config.h"
#include "sources/config_reader.h"
#include "sources/msg.h"
#include "sources/global.h"
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
#include "sources/system.h"
#include "sources/worker.h"
#include "sources/frontend.h"
#include "sources/backend.h"
#include "sources/cron.h"

static inline int
od_cron_stats_server(od_server_t *server, void *arg)
{
	od_serverstat_t *stats = arg;
	stats->count_query   += od_atomic_u64_of(&server->stats.count_query);
	stats->count_tx      += od_atomic_u64_of(&server->stats.count_tx);
	stats->query_time    += od_atomic_u64_of(&server->stats.query_time);
	stats->tx_time       += od_atomic_u64_of(&server->stats.tx_time);
	stats->recv_client   += od_atomic_u64_of(&server->stats.recv_client);
	stats->recv_server   += od_atomic_u64_of(&server->stats.recv_server);
	return 0;
}

static inline void
od_cron_stats(od_router_t *router)
{
	od_instance_t *instance = router->global->instance;

	if (router->route_pool.count == 0)
		return;

	if (instance->config.log_stats)
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
		int      interval    = instance->config.stats_interval;
		uint64_t query       = 0;
		uint64_t tx          = 0;
		uint64_t query_time  = 0;
		uint64_t tx_time     = 0;
		uint64_t recv_client = 0;
		uint64_t recv_server = 0;

		/* ensure server stats not changed due to a
		   server connection close */
		int64_t  query_diff_sanity;
		query_diff_sanity = (stats.count_query - route->cron_stats.count_query);

		if (query_diff_sanity >= 0)
		{
			/* query  */
			uint64_t query_prev    = route->cron_stats.count_query / interval;
			uint64_t query_current = stats.count_query / interval;
			int64_t  query_diff    = query_current - query_prev;
			query = query_diff / interval;
			if (query_diff > 0)
				query_time = (stats.query_time - route->cron_stats.query_time) / query_diff;

			/* transaction  */
			uint64_t tx_prev    = route->cron_stats.count_tx / interval;
			uint64_t tx_current = stats.count_tx / interval;
			int64_t  tx_diff    = tx_current - tx_prev;
			tx = tx_diff / interval;
			if (tx_diff > 0)
				tx_time = (stats.tx_time - route->cron_stats.tx_time) / tx_diff;

			/* recv client */
			uint64_t recv_client_prev    = route->cron_stats.recv_client / interval;
			uint64_t recv_client_current = stats.recv_client / interval;
			recv_client = (recv_client_current - recv_client_prev) / interval;

			/* recv server */
			uint64_t recv_server_prev    = route->cron_stats.recv_server / interval;
			uint64_t recv_server_current = recv_server_current = stats.recv_server / interval;
			recv_server = (recv_server_current - recv_server_prev) / interval;
		}

		/* update current stats */
		route->cron_stats = stats;

		/* update avg stats */
		route->cron_stats_avg.count_query = query;
		route->cron_stats_avg.count_tx    = tx;
		route->cron_stats_avg.query_time  = query_time;
		route->cron_stats_avg.tx_time     = tx_time;
		route->cron_stats_avg.recv_client = recv_client;
		route->cron_stats_avg.recv_server = recv_server;

		if (instance->config.log_stats) {
			od_log(&instance->logger, "stats", NULL, NULL,
			       "[%.*s.%.*s] clients %d, "
			       "pool_active %d, "
			       "pool_idle %d "
			       "rps %" PRIu64 " "
			       "tps %" PRIu64 " "
			       "query_time_us %" PRIu64 " "
			       "tx_time_us %" PRIu64 " "
			       "recv_client_bytes %" PRIu64 " "
			       "recv_server_bytes %" PRIu64,
			       route->id.database_len - 1,
			       route->id.database,
			       route->id.user_len - 1,
			       route->id.user,
			       od_clientpool_total(&route->client_pool),
			       route->server_pool.count_active,
			       route->server_pool.count_idle,
			       query,
			       tx,
			       query_time,
			       tx_time,
			       recv_client,
			       recv_server);
		}
	}
}

static inline int
od_cron_expire_mark(od_server_t *server, void *arg)
{
	od_router_t *router = arg;
	od_instance_t *instance = router->global->instance;
	od_route_t *route = server->route;

	/* expire by time-to-live */
	if (! route->config->pool_ttl)
		return 0;

	od_debug(&instance->logger, "expire", NULL, server,
	         "idle time: %d",
	         server->idle_time);

	if (server->idle_time < route->config->pool_ttl) {
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
	od_router_t *router = cron->global->router;
	od_instance_t *instance = cron->global->instance;

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
	 *  - If a server config marked as obsolete and route has
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

	/* cleanup unused dynamic routes */
	od_routepool_gc(&router->route_pool);
}

static void
od_cron(void *arg)
{
	od_cron_t *cron = arg;
	od_router_t *router = cron->global->router;
	od_instance_t *instance = cron->global->instance;

	int stats_tick = 0;
	for (;;)
	{
		/* mark and sweep expired idle server connections */
		od_cron_expire(cron);

		/* update stats */
		if (++stats_tick >= instance->config.stats_interval) {
			od_cron_stats(router);
			stats_tick = 0;
		}

		/* 1 second soft interval */
		machine_sleep(1000);
	}
}

void od_cron_init(od_cron_t *cron, od_global_t *global)
{
	cron->global = global;
}

int od_cron_start(od_cron_t *cron)
{
	od_instance_t *instance = cron->global->instance;
	int64_t coroutine_id;
	coroutine_id = machine_coroutine_create(od_cron, cron);
	if (coroutine_id == -1) {
		od_error(&instance->logger, "cron", NULL, NULL,
		         "failed to start cron coroutine");
		return -1;
	}
	return 0;
}
