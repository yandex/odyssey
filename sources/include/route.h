#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <types.h>
#include <stat.h>
#include <rules.h>
#include <route_id.h>
#include <client_pool.h>
#include <multi_pool.h>
#include <shared_pool.h>
#include <err_logger.h>
#include <id.h>
#include <route.h>
#include <od_memory.h>

struct od_route {
	od_rule_t *rule;
	od_route_id_t id;

	od_stat_t stats;
	od_stat_t stats_prev;
	bool stats_mark_db;

	/* think twice if you want to work with this fields directly */
	od_shared_pool_t *shared_pool;
	od_multi_pool_t *cached_mpool;
	od_multi_pool_t *exclusive_pool;

	od_client_pool_t client_pool;

	kiwi_params_lock_t params;
	int64_t tcp_connections;
	int last_heartbeat;
	machine_wait_list_t *wait_bus;
	pthread_mutex_t lock;

	od_error_logger_t *err_logger;
	bool extra_logging_enabled;

	od_list_t link;
};

static inline int od_route_init(od_route_t *route, bool extra_route_logging,
				od_shared_pool_t *sp)
{
	route->rule = NULL;
	route->tcp_connections = 0;
	route->last_heartbeat = 0;
	route->exclusive_pool = NULL;
	route->shared_pool = NULL;

	od_route_id_init(&route->id);

	if (sp != NULL) {
		route->shared_pool = od_shared_pool_ref(sp);
	} else {
		route->exclusive_pool = od_multi_pool_create(
			OD_STORAGE_MAX_ENDPOINTS, od_pg_server_pool_free);
		if (route->exclusive_pool == NULL) {
			return NOT_OK_RESPONSE;
		}
	}

	od_client_pool_init(&route->client_pool);

	/* stat init */
	route->stats_mark_db = false;
	route->extra_logging_enabled = extra_route_logging;
	if (extra_route_logging) {
		/* error logging */
		route->err_logger = od_err_logger_create_default();
	} else {
		route->err_logger = NULL;
	}

	od_stat_init(&route->stats);
	od_stat_init(&route->stats_prev);
	kiwi_params_lock_init(&route->params);
	od_list_init(&route->link);
	route->wait_bus = NULL;
	pthread_mutex_init(&route->lock, NULL);

	return OK_RESPONSE;
}

static inline void od_route_free(od_route_t *route)
{
	od_route_id_free(&route->id);

	if (route->exclusive_pool != NULL) {
		od_multi_pool_destroy(route->exclusive_pool);
	} else {
		od_shared_pool_unref(route->shared_pool);
	}

	kiwi_params_lock_free(&route->params);
	if (route->wait_bus) {
		machine_wait_list_destroy(route->wait_bus);
	}
	if (route->stats.enable_quantiles) {
		od_stat_free(&route->stats);
	}

	if (route->extra_logging_enabled) {
		od_err_logger_free(route->err_logger);
		route->err_logger = NULL;
	}

	pthread_mutex_destroy(&route->lock);
	od_free(route);
}

static inline od_route_t *od_route_allocate(od_rule_t *rule)
{
	od_route_t *route = od_malloc(sizeof(od_route_t));
	if (route == NULL) {
		return NULL;
	}
	if (od_route_init(route, true, rule->shared_pool) != OK_RESPONSE) {
		od_route_free(route);
		return NULL;
	}
	route->wait_bus = machine_wait_list_create(NULL);
	if (route->wait_bus == NULL) {
		od_route_free(route);
		return NULL;
	}
	return route;
}

static inline void od_route_lock(od_route_t *route)
{
	pthread_mutex_lock(&route->lock);
}

static inline void od_route_unlock(od_route_t *route)
{
	pthread_mutex_unlock(&route->lock);
}

static inline int od_route_is_dynamic(od_route_t *route)
{
	return route->rule->db_is_default || route->rule->user_is_default;
}

static inline int od_route_match_compare_client_cb(od_client_t *client,
						   void **argv)
{
	return od_id_cmp(&client->id, argv[0]);
}

static inline od_multi_pool_t *od_route_get_multi_pool(od_route_t *route)
{
	if (route->exclusive_pool != NULL) {
		return route->exclusive_pool;
	}

	if (route->cached_mpool != NULL) {
		return route->cached_mpool;
	}

	assert(route->id.database[route->id.database_len] == '\0');
	assert(route->id.user[route->id.user_len] == '\0');

	route->cached_mpool = od_shared_pool_get_or_create(
		route->shared_pool, route->id.database, route->id.user);

	return route->cached_mpool;
}

static inline int od_route_total_servers(od_route_t *route)
{
	if (route->exclusive_pool != NULL) {
		return od_multi_pool_total(route->exclusive_pool);
	}

	return od_shared_pool_total(route->shared_pool);
}

static inline od_client_t *od_route_match_client(od_route_t *route, od_id_t *id)
{
	void *argv[] = { id };
	od_client_t *match;
	match = od_client_pool_foreach(&route->client_pool, OD_CLIENT_ACTIVE,
				       od_route_match_compare_client_cb, argv);
	if (match) {
		return match;
	}
	match = od_client_pool_foreach(&route->client_pool, OD_CLIENT_QUEUE,
				       od_route_match_compare_client_cb, argv);
	if (match) {
		return match;
	}
	match = od_client_pool_foreach(&route->client_pool, OD_CLIENT_PENDING,
				       od_route_match_compare_client_cb, argv);
	if (match) {
		return match;
	}

	return NULL;
}

static inline void od_route_kill_client(od_route_t *route, od_id_t *id)
{
	od_client_t *client;
	client = od_route_match_client(route, id);
	if (client) {
		od_client_kill(client);
	}
}

static inline int od_route_kill_cb(od_client_t *client, void **argv)
{
	(void)argv;
	od_client_kill(client);
	return 0;
}

static inline int od_grac_shutdown_cb(od_server_t *server, void **argv)
{
	(void)argv;
	od_server_grac_shutdown(server);
	return 0;
}

static inline void od_route_kill_client_pool(od_route_t *route)
{
	od_client_pool_foreach(&route->client_pool, OD_CLIENT_ACTIVE,
			       od_route_kill_cb, NULL);
	od_client_pool_foreach(&route->client_pool, OD_CLIENT_PENDING,
			       od_route_kill_cb, NULL);
	od_client_pool_foreach(&route->client_pool, OD_CLIENT_QUEUE,
			       od_route_kill_cb, NULL);
}

static inline void od_route_grac_shutdown_pool(od_route_t *route)
{
	od_multi_pool_t *mpool = od_route_get_multi_pool(route);
	od_multi_pool_foreach(mpool, OD_SERVER_ACTIVE, od_grac_shutdown_cb,
			      NULL);
	od_multi_pool_foreach(mpool, OD_SERVER_IDLE, od_grac_shutdown_cb, NULL);
}

static inline int od_route_wait(od_route_t *route, uint32_t time_ms)
{
	int rc;
	rc = machine_wait_list_wait(route->wait_bus, time_ms);
	return rc;
}

static inline int od_route_signal(od_route_t *route)
{
	machine_wait_list_notify(route->wait_bus);
	return 0;
}
