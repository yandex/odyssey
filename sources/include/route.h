#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stdatomic.h>

#include <types.h>
#include <stat.h>
#include <rules.h>
#include <route_id.h>
#include <client_pool.h>
#include <multi_pool.h>
#include <err_logger.h>
#include <id.h>
#include <shared_pool.h>
#include <od_memory.h>

struct od_route {
	od_rule_t *rule;
	od_route_id_t id;

	od_stat_t stats;
	od_stat_t stats_prev;
	bool stats_mark_db;

	/*
	 * only one must be non NULL
	 */
	od_multi_pool_t *exclusive_pool;
	od_shared_pool_t *shared_pool;

	od_client_pool_t client_pool;

	kiwi_params_lock_t params;
	int64_t tcp_connections;
	int last_heartbeat;
	pthread_mutex_t lock;

	od_error_logger_t *err_logger;
	bool extra_logging_enabled;

	od_list_t link;
};

static inline int od_route_has_shared_pool(od_route_t *route)
{
	assert((route->shared_pool != NULL) == (route->exclusive_pool == NULL));
	return route->shared_pool != NULL;
}

static inline int od_route_has_exclusive_pool(od_route_t *route)
{
	assert((route->exclusive_pool != NULL) == (route->shared_pool == NULL));
	return route->exclusive_pool != NULL;
}

static inline od_multi_pool_t *od_route_server_pools(od_route_t *route)
{
	if (od_route_has_exclusive_pool(route)) {
		assert(route->shared_pool == NULL);
		return route->exclusive_pool;
	}

	assert(route->exclusive_pool == NULL);
	return route->shared_pool->mpool;
}

static inline int od_multi_pool_filter_by_route(void *arg,
						const od_multi_pool_key_t *key)
{
	od_route_t *route = arg;

	if (key->dbname == NULL ||
	    strcmp(route->id.database, key->dbname) != 0) {
		return 0;
	}

	if (key->username == NULL ||
	    strcmp(route->id.user, key->username) != 0) {
		return 0;
	}

	return 1;
}

static inline od_server_t *
od_route_server_pool_foreach_locked(od_route_t *route, od_server_state_t state,
				    od_server_pool_cb_t callback, void **argv)
{
	if (route->shared_pool == NULL) {
		return od_multi_pool_foreach_locked(route->exclusive_pool, NULL,
						    NULL, state, callback,
						    argv);
	}

	return od_multi_pool_foreach_locked(route->shared_pool->mpool,
					    od_multi_pool_filter_by_route,
					    route, state, callback, argv);
}

static inline int od_route_server_pool_count_active_locked(od_route_t *route,
							   int force_id)
{
	if (route->shared_pool == NULL) {
		return od_multi_pool_count_active_locked(route->exclusive_pool,
							 NULL, NULL);
	}

	if (force_id) {
		return od_multi_pool_count_active_locked(
			route->shared_pool->mpool,
			od_multi_pool_filter_by_route, route);
	}

	return od_multi_pool_count_active_locked(route->shared_pool->mpool,
						 NULL, NULL);
}

static inline int od_route_server_pool_count_idle_locked(od_route_t *route,
							 int force_id)
{
	if (route->shared_pool == NULL) {
		return od_multi_pool_count_idle_locked(route->exclusive_pool,
						       NULL, NULL);
	}

	if (force_id) {
		return od_multi_pool_count_idle_locked(
			route->shared_pool->mpool,
			od_multi_pool_filter_by_route, route);
	}

	return od_multi_pool_count_idle_locked(route->shared_pool->mpool, NULL,
					       NULL);
}

static inline int od_route_server_pool_count_total_locked(od_route_t *route,
							  int force_id)
{
	if (route->shared_pool == NULL) {
		return od_multi_pool_total_locked(route->exclusive_pool, NULL,
						  NULL);
	}

	if (force_id) {
		return od_multi_pool_total_locked(route->shared_pool->mpool,
						  od_multi_pool_filter_by_route,
						  route);
	}

	return od_multi_pool_total_locked(route->shared_pool->mpool, NULL,
					  NULL);
}

static inline int od_route_init(od_route_t *route,
				od_shared_pool_t *shared_pool,
				bool extra_route_logging)
{
	route->rule = NULL;
	route->tcp_connections = 0;
	route->last_heartbeat = 0;

	od_route_id_init(&route->id);

	route->shared_pool = NULL;
	route->exclusive_pool = NULL;

	if (shared_pool == NULL) {
		route->exclusive_pool =
			od_multi_pool_create(od_pg_server_pool_free);
		if (route->exclusive_pool == NULL) {
			return NOT_OK_RESPONSE;
		}
	} else {
		route->shared_pool = od_shared_pool_ref(shared_pool);
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

static inline od_route_t *od_route_allocate(od_shared_pool_t *shared_pool)
{
	od_route_t *route = od_malloc(sizeof(od_route_t));
	if (route == NULL) {
		return NULL;
	}
	if (od_route_init(route, shared_pool, true /* extra_route_logging */) !=
	    OK_RESPONSE) {
		od_route_free(route);
		return NULL;
	}
	return route;
}

static inline void od_route_lock(od_route_t *route)
{
	pthread_mutex_lock(&route->lock);

	od_multi_pool_lock(od_route_server_pools(route));
}

static inline void od_route_unlock(od_route_t *route)
{
	od_multi_pool_unlock(od_route_server_pools(route));

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

static inline void od_route_kill_client_pool(od_route_t *route)
{
	od_client_pool_foreach(&route->client_pool, OD_CLIENT_ACTIVE,
			       od_route_kill_cb, NULL);
	od_client_pool_foreach(&route->client_pool, OD_CLIENT_PENDING,
			       od_route_kill_cb, NULL);
	od_client_pool_foreach(&route->client_pool, OD_CLIENT_QUEUE,
			       od_route_kill_cb, NULL);
}

static inline uint64_t od_route_pools_version(od_route_t *route)
{
	od_multi_pool_t *mpool = od_route_server_pools(route);

	return od_multi_pool_version(mpool);
}

static inline int od_route_wait(od_route_t *route, uint64_t version,
				uint32_t timeout_ms)
{
	od_multi_pool_t *mpool = od_route_server_pools(route);

	return od_multi_pool_wait(mpool, version, timeout_ms);
}

static inline int od_route_signal(od_route_t *route)
{
	od_multi_pool_t *mpool = od_route_server_pools(route);

	return od_multi_pool_signal(mpool);
}

int od_route_server_pool_can_add_locked(od_route_t *route,
					od_multi_pool_element_t *el);

int od_route_server_pool_total(od_route_t *route, od_multi_pool_element_t *el);

int od_route_server_pool_next_idle_locked(od_route_t *route,
					  const od_address_t *address,
					  od_server_t **server);

od_multi_pool_element_t *
od_route_get_server_pool_element_locked(od_route_t *route,
					const od_address_t *address);
