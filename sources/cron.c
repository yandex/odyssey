
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

static int
od_cron_stat_cb(od_route_t *route, od_stat_t *current, od_stat_t *avg,
                void *arg)
{
	od_router_t *router = arg;
	od_instance_t *instance = router->global->instance;

	/* update route stats */
	route->stats_prev = *current;

	if (! instance->config.log_stats)
		return 0;

	od_log(&instance->logger, "stats", NULL, NULL,
	       "[%.*s.%.*s] %d clients, "
	       "%d active servers, "
	       "%d idle servers, "
	       "%" PRIu64 " transactions/sec (%" PRIu64 " usec) "
	       "%" PRIu64 " queries/sec (%"  PRIu64 " usec) "
	       "%" PRIu64 " in bytes/sec, "
	       "%" PRIu64 " out bytes/sec",
	       route->id.database_len - 1,
	       route->id.database,
	       route->id.user_len - 1,
	       route->id.user,
	       od_client_pool_total(&route->client_pool),
	       route->server_pool.count_active,
	       route->server_pool.count_idle,
	       avg->count_tx,
	       avg->tx_time,
	       avg->count_query,
	       avg->query_time,
	       avg->recv_client,
	       avg->recv_server);

	return 0;
}

static inline void
od_cron_stat(od_cron_t *cron, od_router_t *router)
{
	od_instance_t *instance = router->global->instance;

	if (instance->config.log_stats)
	{
		uint64_t count_machine = 0;
		uint64_t count_coroutine = 0;
		uint64_t count_coroutine_cache = 0;
		uint64_t msg_allocated = 0;
		uint64_t msg_cache_count = 0;
		uint64_t msg_cache_gc_count = 0;
		uint64_t msg_cache_size = 0;
		machinarium_stat(&count_machine, &count_coroutine,
		                 &count_coroutine_cache,
		                 &msg_allocated,
		                 &msg_cache_count,
		                 &msg_cache_gc_count,
		                 &msg_cache_size);
		od_log(&instance->logger, "stats", NULL, NULL,
		       "msg (%" PRIu64 " allocated, %" PRIu64 " gc, %" PRIu64 " cached, %" PRIu64 " cache_size), "
		       "coroutines (%" PRIu64 " active, %"PRIu64 " cached)",
		       msg_allocated,
		       msg_cache_gc_count,
		       msg_cache_count,
		       msg_cache_size,
		       count_coroutine,
		       count_coroutine_cache);

		od_log(&instance->logger, "stats", NULL, NULL,
		       "clients %d", router->clients);
	}

	if (router->route_pool.count == 0)
		return;

	/* update stats per route */
	od_route_pool_stat(&router->route_pool, od_cron_stat_cb,
	                   cron->stat_time_us,
	                   router);

	/* update current stat time mark */
	cron->stat_time_us = machine_time();
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
	od_server_pool_set(&route->server_pool, server,
	                   OD_SERVER_EXPIRE);
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
	od_route_pool_server_foreach(&router->route_pool, OD_SERVER_IDLE,
	                             od_cron_expire_mark,
	                             router);

	/* sweep */
	for (;;)
	{
		od_server_t *server;
		server = od_route_pool_next(&router->route_pool, OD_SERVER_EXPIRE);
		if (server == NULL)
			break;
		od_debug(&instance->logger, "expire", NULL, server,
		         "closing idle server connection (%d secs)",
		         server->idle_time);
		server->idle_time = 0;

		od_route_t *route = server->route;
		server->route = NULL;
		od_server_pool_set(&route->server_pool, server, OD_SERVER_UNDEF);

		if (instance->is_shared)
			machine_io_attach(server->io);

		od_backend_close_connection(server);
		od_backend_close(server);
	}

	/* cleanup unused dynamic routes */
	od_route_pool_gc(&router->route_pool);
}

static void
od_cron(void *arg)
{
	od_cron_t *cron = arg;
	od_router_t *router = cron->global->router;
	od_instance_t *instance = cron->global->instance;

	cron->stat_time_us = machine_time();

	int stats_tick = 0;
	for (;;)
	{
		/* mark and sweep expired idle server connections */
		od_cron_expire(cron);

		/* update statistics */
		if (++stats_tick >= instance->config.stats_interval) {
			od_cron_stat(cron, router);
			stats_tick = 0;
		}

		/* 1 second soft interval */
		machine_sleep(1000);
	}
}

void
od_cron_init(od_cron_t *cron, od_global_t *global)
{
	cron->global = global;
	cron->stat_time_us = 0;
}

int
od_cron_start(od_cron_t *cron)
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
