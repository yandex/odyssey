
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium/machinarium.h>
#include <odyssey.h>
#include <stdlib.h>
#include <stdio.h>

static int od_cron_stat_cb(od_route_t *route, od_stat_t *current,
			   od_stat_t *avg,
#ifdef PROM_FOUND
			   od_prom_metrics_t *metrics,
#endif
			   void **argv)
{
	od_instance_t *instance = argv[0];
	(void)current;

	struct {
		int database_len;
		char database[64];
		int user_len;
		char user[64];
		int obsolete;
		int client_pool_total;
		int server_pool_active;
		int server_pool_idle;
		uint64_t avg_count_tx;
		uint64_t avg_tx_time;
		uint64_t avg_count_query;
		uint64_t avg_query_time;
		uint64_t avg_recv_client;
		uint64_t avg_recv_server;
	} info;

	od_route_lock(route);

	info.database_len = od_snprintf(info.database, sizeof(info.database),
					"%s", route->rule->db_name);
	info.user_len = od_snprintf(info.user, sizeof(info.user), "%s",
				    route->rule->user_name);

	info.obsolete = route->rule->obsolete;
	info.client_pool_total = od_client_pool_total(&route->client_pool);
	info.server_pool_active =
		od_multi_pool_count_active(route->server_pools);
	info.server_pool_idle = od_multi_pool_count_idle(route->server_pools);

	info.avg_count_query = avg->count_query;
	info.avg_count_tx = avg->count_tx;
	info.avg_query_time = avg->query_time;
	info.avg_tx_time = avg->tx_time;
	info.avg_recv_server = avg->recv_server;
	info.avg_recv_client = avg->recv_client;

	od_route_unlock(route);

#ifdef PROM_FOUND
	if (instance->config.log_general_stats_prom) {
		od_prom_metrics_write_stat_cb(
			metrics, info.user, info.database, info.database_len,
			info.user_len, info.client_pool_total,
			info.server_pool_active, info.server_pool_idle,
			info.avg_count_tx, info.avg_tx_time,
			info.avg_count_query, info.avg_query_time,
			info.avg_recv_client, info.avg_recv_server);
		if (instance->config.log_route_stats_prom) {
			char *prom_log =
				(char *)od_prom_metrics_get_stat_cb(metrics);
			od_logger_write_plain(&instance->logger, OD_LOG,
					      "stats", NULL, NULL, prom_log);
			od_free(prom_log);
		}
	}
#endif
	od_log(&instance->logger, "stats", NULL, NULL,
	       "[%.*s.%.*s%s] %d clients, "
	       "%d active servers, "
	       "%d idle servers, "
	       "%" PRIu64 " transactions/sec (%" PRIu64 " usec) "
	       "%" PRIu64 " queries/sec (%" PRIu64 " usec) "
	       "%" PRIu64 " in bytes/sec, "
	       "%" PRIu64 " out bytes/sec",
	       info.database_len, info.database, info.user_len, info.user,
	       info.obsolete ? " obsolete" : "", info.client_pool_total,
	       info.server_pool_active, info.server_pool_idle,
	       info.avg_count_tx, info.avg_tx_time, info.avg_count_query,
	       info.avg_query_time, info.avg_recv_client, info.avg_recv_server);

	return 0;
}

static inline void send_msg_stat(machine_channel_t *chan)
{
	machine_msg_t *msg;
	msg = machine_msg_create(0);
	machine_msg_set_type(msg, OD_MSG_STAT);
	machine_channel_write(chan, msg);
}

static inline void request_logger_stats(od_logger_t *logger)
{
	send_msg_stat(logger->task_channel);
}

static inline void request_worker_stats(od_worker_pool_t *worker_pool)
{
	uint32_t i;
	for (i = 0; i < worker_pool->count; i++) {
		od_worker_t *worker = &worker_pool->pool[i];
		send_msg_stat(worker->task_channel);
	}
}

static inline void od_cron_stat(od_cron_t *cron)
{
	od_router_t *router = cron->global->router;
	od_instance_t *instance = cron->global->instance;
	od_worker_pool_t *worker_pool = cron->global->worker_pool;

	if (instance->config.log_stats) {
		/* system worker stats */
		uint64_t count_coroutine = 0;
		uint64_t count_coroutine_cache = 0;
		uint64_t msg_allocated = 0;
		uint64_t msg_cache_count = 0;
		uint64_t msg_cache_gc_count = 0;
		uint64_t msg_cache_size = 0;

		od_atomic_u64_t startup_errors =
			od_atomic_u64_of(&cron->startup_errors);
		cron->startup_errors = 0;
		machine_stat(&count_coroutine, &count_coroutine_cache,
			     &msg_allocated, &msg_cache_count,
			     &msg_cache_gc_count, &msg_cache_size);
#ifdef PROM_FOUND
		if (instance->config.log_general_stats_prom) {
			od_prom_metrics_write_stat(
				cron->metrics, msg_allocated, msg_cache_count,
				msg_cache_gc_count, msg_cache_size,
				count_coroutine, count_coroutine_cache);
			char *prom_log =
				(char *)od_prom_metrics_get_stat(cron->metrics);
			od_logger_write_plain(&instance->logger, OD_LOG,
					      "stats", NULL, NULL, prom_log);
			od_free(prom_log);
		}
#endif
		od_log(&instance->logger, "stats", NULL, NULL,
		       "system worker: msg (%" PRIu64 " allocated, %" PRIu64
		       " cached, %" PRIu64 " freed, %" PRIu64 " cache_size), "
		       "coroutines (%" PRIu64 " active, %" PRIu64
		       " cached) startup errors %" PRIu64,
		       msg_allocated, msg_cache_count, msg_cache_gc_count,
		       msg_cache_size, count_coroutine, count_coroutine_cache,
		       startup_errors);

		/* request stats per worker */
		request_worker_stats(worker_pool);

		request_logger_stats(&instance->logger);

		od_log(&instance->logger, "stats", NULL, NULL, "clients %d",
		       od_atomic_u32_of(&router->clients));
	}

	/* update stats per route and print info */
	od_route_pool_stat_cb_t stat_cb;
	if (!instance->config.log_stats) {
		stat_cb = NULL;
	} else {
		stat_cb = od_cron_stat_cb;
	}
	void *argv[] = { instance };
	od_router_stat(router, cron->stat_time_us,
#ifdef PROM_FOUND
		       cron->metrics,
#endif
		       stat_cb, argv);

	/* update current stat time mark */
	cron->stat_time_us = machine_time_us();
}

static inline void od_cron_keep_min_pool_sizes(od_cron_t *cron)
{
	od_router_t *router = cron->global->router;
	od_router_keep_min_pool_size_step(router);
}

static void od_rules_gc()
{
	/* remove all obsolete rules that has no refs on it */
	od_global_t *global = od_global_get();
	od_router_t *router = global->router;

	/*
	 * we will do it in two steps:
	 * - create list of obsolete rules
	 * - remove all rules from list
	 *
	 * thats because i dont want to perform free() with locked router
	 */
	od_list_t to_delete;
	od_list_init(&to_delete);

	od_router_lock(router);

	od_list_t *i, *n;
	od_list_foreach_safe(&router->rules.rules, i, n)
	{
		od_rule_t *rule = od_container_of(i, od_rule_t, link);

		if (!rule->obsolete) {
			continue;
		}

		if (rule->refs > 0) {
			continue;
		}

		od_list_unlink(&rule->link);
		od_list_append(&to_delete, &rule->link);
	}

	od_router_unlock(router);

	od_list_foreach_safe(&to_delete, i, n)
	{
		od_rule_t *rule = od_container_of(i, od_rule_t, link);

		od_list_unlink(&rule->link);
		od_rules_rule_free(rule);
	}
}

static inline void od_cron_expire(od_cron_t *cron)
{
	od_router_t *router = cron->global->router;

	od_instance_t *instance = cron->global->instance;

	/* collect and close expired idle servers */
	od_list_t expire_list;
	od_list_init(&expire_list);

	/*
	 * TODO: close connections in some other thread?
	 * Now closing several connections at once can take some long time (why?)
	 * and lead to pool_ttl or server_lifetime violation on the order of a seconds
	 */
	int rc;
	rc = od_router_expire(router, &expire_list);
	if (rc > 0) {
		od_list_t *i, *n;
		od_list_foreach_safe(&expire_list, i, n)
		{
			od_server_t *server;
			server = od_container_of(i, od_server_t, link);
			od_debug(&instance->logger, "expire", NULL, server,
				 "closing idle server connection (%d secs)",
				 server->idle_time);
			server->route = NULL;
			od_backend_close_connection(server);
			od_backend_close(server);
		}
	}

	/* cleanup unused dynamic or obsolete routes */
	od_router_gc(router);

	/* cleanup unused rules */
	od_rules_gc();
}

static void od_cron_err_stat(od_cron_t *cron)
{
	od_router_t *router = cron->global->router;

	od_list_t *it;
	od_list_foreach(&router->route_pool.list, it)
	{
		od_route_t *current_route =
			od_container_of(it, od_route_t, link);
		od_route_lock(current_route);
		{
			if (current_route->extra_logging_enabled) {
				od_err_logger_inc_interval(
					current_route->err_logger);
			}
		}
		od_route_unlock(current_route);
	}

	od_router_lock(router)
	{
		od_err_logger_inc_interval(router->router_err_logger);
	}
	od_router_unlock(router)

		od_route_pool_lock(router->route_pool)
	{
		od_err_logger_inc_interval(router->route_pool.err_logger);
	}
	od_route_pool_unlock(router->route_pool)
}

static void od_cron(void *arg)
{
	od_cron_t *cron = arg;
	od_instance_t *instance = cron->global->instance;

	cron->stat_time_us = machine_time_us();
	atomic_store(&cron->online, 1);

	int stats_tick = 0;
	for (;;) {
		if (!atomic_load(&cron->online)) {
			break;
		}

		/* mark and sweep expired idle server connections */
		od_cron_expire(cron);

		/* create server connections if pool size is less than min_pool_size */
		od_cron_keep_min_pool_sizes(cron);

		/* update statistics */
		if (++stats_tick >= instance->config.stats_interval) {
			od_cron_stat(cron);
			stats_tick = 0;
		}

		od_cron_err_stat(cron);

		/* 1 second soft interval */
		machine_sleep(1000);
	}

	/*
	 * a wait flag is used to prevent usage of routes
	 * that are deallocated during shutdown
	 */
	machine_wait_flag_set(cron->can_be_freed);
}

od_retcode_t od_cron_init(od_cron_t *cron)
{
	cron->stat_time_us = 0;
	cron->global = NULL;
	cron->startup_errors = 0;

#ifdef PROM_FOUND
	cron->metrics =
		(od_prom_metrics_t *)od_malloc(sizeof(od_prom_metrics_t));
	cron->metrics->port = 0;
	cron->metrics->http_server = NULL;
#endif

	atomic_init(&cron->online, 0);
	cron->can_be_freed = machine_wait_flag_create();
	if (cron->can_be_freed == NULL) {
		return -1;
	}
	return 0;
}

int od_cron_start(od_cron_t *cron, od_global_t *global)
{
	cron->global = global;
	od_instance_t *instance = global->instance;
	int64_t coroutine_id;
	coroutine_id = machine_coroutine_create(od_cron, cron);
	if (coroutine_id == INVALID_COROUTINE_ID) {
		od_error(&instance->logger, "cron", NULL, NULL,
			 "failed to start cron coroutine");
		return NOT_OK_RESPONSE;
	}

	return OK_RESPONSE;
}

od_retcode_t od_cron_stop(od_cron_t *cron)
{
	atomic_store(&cron->online, 0);
	machine_wait_flag_wait(cron->can_be_freed, UINT32_MAX);
#ifdef PROM_FOUND
	od_prom_metrics_destroy(cron->metrics);
#endif
	return OK_RESPONSE;
}
