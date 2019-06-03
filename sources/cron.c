
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

#include <odyssey.h>

static int
od_cron_stat_cb(od_route_t *route, od_stat_t *current, od_stat_t *avg,
                void **argv)
{
	od_instance_t *instance = argv[0];
	(void)current;

	struct {
		int      database_len;
		char     database[64];
		int      user_len;
		char     user[64];
		int      obsolete;
		int      client_pool_total;
		int      server_pool_active;
		int      server_pool_idle;
		uint64_t avg_count_tx;
		uint64_t avg_tx_time;
		uint64_t avg_count_query;
		uint64_t avg_query_time;
		uint64_t avg_recv_client;
		uint64_t avg_recv_server;
	} info;

	od_route_lock(route);

	info.database_len = route->id.database_len - 1;
	if (info.database_len > (int)sizeof(info.database))
		info.database_len = sizeof(info.database);

	info.user_len = route->id.user_len - 1;
	if (info.user_len > (int)sizeof(info.user))
		info.user_len = sizeof(info.user);

	memcpy(info.database, route->id.database, route->id.database_len);
	memcpy(info.user, route->id.user, route->id.user_len);

	info.obsolete           = route->rule->obsolete;
	info.client_pool_total  = od_client_pool_total(&route->client_pool);
	info.server_pool_active = route->server_pool.count_active;
	info.server_pool_idle   = route->server_pool.count_idle;

	info.avg_count_query    = avg->count_query;
	info.avg_count_tx       = avg->count_tx;
	info.avg_query_time     = avg->query_time;
	info.avg_tx_time        = avg->tx_time;
	info.avg_recv_server    = avg->recv_server;
	info.avg_recv_client    = avg->recv_client;

	od_route_unlock(route);

	od_log(&instance->logger, "stats", NULL, NULL,
	       "[%.*s.%.*s%s] %d clients, "
	       "%d active servers, "
	       "%d idle servers, "
	       "%" PRIu64 " transactions/sec (%" PRIu64 " usec) "
	       "%" PRIu64 " queries/sec (%"  PRIu64 " usec) "
	       "%" PRIu64 " in bytes/sec, "
	       "%" PRIu64 " out bytes/sec",
	       info.database_len,
	       info.database,
	       info.user_len,
	       info.user,
	       info.obsolete ? " obsolete" : "",
	       info.client_pool_total,
	       info.server_pool_active,
	       info.server_pool_idle,
	       info.avg_count_tx,
	       info.avg_tx_time,
	       info.avg_count_query,
	       info.avg_query_time,
	       info.avg_recv_client,
	       info.avg_recv_server);

	return 0;
}

static inline void
od_cron_stat(od_cron_t *cron)
{
	od_router_t *router = cron->global->router;
	od_instance_t *instance = cron->global->instance;
	od_worker_pool_t *worker_pool = cron->global->worker_pool;

	if (instance->config->log_stats)
	{
		/* system worker stats */
		uint64_t count_coroutine = 0;
		uint64_t count_coroutine_cache = 0;
		uint64_t msg_allocated = 0;
		uint64_t msg_cache_count = 0;
		uint64_t msg_cache_gc_count = 0;
		uint64_t msg_cache_size = 0;
		machine_stat(&count_coroutine,
		             &count_coroutine_cache,
		             &msg_allocated,
		             &msg_cache_count,
		             &msg_cache_gc_count,
		             &msg_cache_size);
		od_log(&instance->logger, "stats", NULL, NULL,
		       "system worker: msg (%" PRIu64 " allocated, %" PRIu64 " cached, %" PRIu64 " freed, %" PRIu64 " cache_size), "
		       "coroutines (%" PRIu64 " active, %"PRIu64 " cached)",
		       msg_allocated,
		       msg_cache_count,
		       msg_cache_gc_count,
		       msg_cache_size,
		       count_coroutine,
		       count_coroutine_cache);

		/* request stats per worker */
		int i;
		for (i = 0; i < worker_pool->count; i++) {
			od_worker_t *worker = &worker_pool->pool[i];
			machine_msg_t *msg;
			msg = machine_msg_create(0);
			machine_msg_set_type(msg, OD_MSG_STAT);
			machine_channel_write(worker->task_channel, msg);
		}

		od_log(&instance->logger, "stats", NULL, NULL,
		       "clients %d", router->clients);
	}

	/* update stats per route and print info */
	od_route_pool_stat_cb_t stat_cb;
	stat_cb = od_cron_stat_cb;
	if (! instance->config->log_stats)
		stat_cb = NULL;
	void *argv[] = { instance };
	od_router_stat(router, cron->stat_time_us, 1, stat_cb, argv);

	/* update current stat time mark */
	cron->stat_time_us = machine_time_us();
}

static inline void
od_cron_expire(od_cron_t *cron)
{
	od_router_t *router = cron->global->router;
	od_instance_t *instance = cron->global->instance;

	/* collect and close expired idle servers */
	od_list_t expire_list;
	od_list_init(&expire_list);

	int rc;
	rc = od_router_expire(router, &expire_list);
	if (rc > 0)
	{
		od_list_t *i, *n;
		od_list_foreach_safe(&expire_list, i, n) {
			od_server_t *server;
			server = od_container_of(i, od_server_t, link);
			od_debug(&instance->logger, "expire", NULL, server,
			         "closing idle server connection (%d secs)",
			         server->idle_time);
			server->route = NULL;
			if (! od_config_is_multi_workers(instance->config))
				od_io_attach(&server->io);
			od_backend_close_connection(server);
			od_backend_close(server);
		}
	}

	/* cleanup unused dynamic or obsolete routes */
	od_router_gc(router);
}

static void
od_cron(void *arg)
{
	od_cron_t *cron = arg;
	od_instance_t *instance = cron->global->instance;

	cron->stat_time_us = machine_time_us();

	int stats_tick = 0;
	for (;;)
	{
		/* mark and sweep expired idle server connections */
		od_cron_expire(cron);

		/* update statistics */
		if (++stats_tick >= instance->config->stats_interval) {
			od_cron_stat(cron);
			stats_tick = 0;
		}

		/* 1 second soft interval */
		machine_sleep(1000);
	}
}

void
od_cron_init(od_cron_t *cron)
{
	cron->stat_time_us = 0;
	cron->global = NULL;
}

int
od_cron_start(od_cron_t *cron, od_global_t *global)
{
	cron->global = global;
	od_instance_t *instance = global->instance;
	int64_t coroutine_id;
	coroutine_id = machine_coroutine_create(od_cron, cron);
	if (coroutine_id == -1) {
		od_error(&instance->logger, "cron", NULL, NULL,
		         "failed to start cron coroutine");
		return -1;
	}
	return 0;
}
