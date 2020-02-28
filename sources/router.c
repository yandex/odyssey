
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
#include <misc.h>

void
od_router_init(od_router_t *router)
{
	pthread_mutex_init(&router->lock, NULL);
	od_rules_init(&router->rules);
	od_route_pool_init(&router->route_pool);
	router->clients = 0;
	router->clients_routing = 0;
	router->servers_routing = 0;
}

void
od_router_free(od_router_t *router)
{
	od_route_pool_free(&router->route_pool);
	od_rules_free(&router->rules);
	pthread_mutex_destroy(&router->lock);
}

inline int
od_router_foreach(od_router_t *router,
                  od_route_pool_cb_t callback,
                  void **argv)
{
	od_router_lock(router);
	int rc;
	rc = od_route_pool_foreach(&router->route_pool, callback, argv);
	od_router_unlock(router);
	return rc;
}

static inline int
od_router_kill_clients_cb(od_route_t *route, void **argv)
{
	(void)argv;
	if (! route->rule->obsolete)
		return 0;
	od_route_lock(route);
	od_route_kill_client_pool(route);
	od_route_unlock(route);
	return 0;
}

int
od_router_reconfigure(od_router_t *router, od_rules_t *rules)
{
	od_router_lock(router);

	int updates;
	updates = od_rules_merge(&router->rules, rules);

	if (updates > 0) {
		od_route_pool_foreach(&router->route_pool, od_router_kill_clients_cb,
		                      NULL);
	}

	od_router_unlock(router);
	return updates;
}

static inline int
od_router_expire_server_cb(od_server_t *server, void **argv)
{
	od_route_t *route = server->route;
	od_list_t *expire_list = argv[0];
	int *count = argv[1];

	/* remove server for server pool */
	od_server_pool_set(&route->server_pool, server, OD_SERVER_UNDEF);

	od_list_append(expire_list, &server->link);
	(*count)++;

	return 0;
}

static inline int
od_router_expire_server_tick_cb(od_server_t *server, void **argv)
{
	od_route_t *route = server->route;
	od_list_t *expire_list = argv[0];
	int *count = argv[1];
    uint64_t *now_us = argv[2];

    uint64_t lifetime = route->rule->server_lifetime_us;
    uint64_t server_life = *now_us - server->init_time_us;

	/* advance idle time for 1 sec */
	if (server_life < lifetime && server->idle_time < route->rule->pool_ttl) {
		server->idle_time++;
		return 0;
	}

	/*
	 * Do not expire more servers than we are allowed to connect at one time
	 * This avoids need to re-launch lot of connections together
	 */
	if (*count > route->rule->storage->server_max_routing)
		return 0;

	/* remove server for server pool */
	od_server_pool_set(&route->server_pool, server, OD_SERVER_UNDEF);

	/* add to expire list */
	od_list_append(expire_list, &server->link);
	(*count)++;

	return 0;
}

static inline int
od_router_expire_cb(od_route_t *route, void **argv)
{
	od_route_lock(route);

	/* expire by config obsoletion */
	if (route->rule->obsolete && !od_client_pool_total(&route->client_pool))
	{
		od_server_pool_foreach(&route->server_pool,
		                       OD_SERVER_IDLE,
		                       od_router_expire_server_cb,
		                       argv);

		od_route_unlock(route);
		return 0;
	}

	if (! route->rule->pool_ttl) {
		od_route_unlock(route);
		return 0;
	}

	od_server_pool_foreach(&route->server_pool,
	                       OD_SERVER_IDLE,
	                       od_router_expire_server_tick_cb,
	                       argv);

	od_route_unlock(route);
	return 0;
}

int
od_router_expire(od_router_t *router, od_list_t *expire_list)
{
	int count = 0;
	uint64_t now_us = machine_time_us();
	void *argv[] = { expire_list, &count, &now_us };
	od_router_foreach(router, od_router_expire_cb, argv);
	return count;
}

static inline int
od_router_gc_cb(od_route_t *route, void **argv)
{
	od_route_pool_t *pool = argv[0];
	od_route_lock(route);

	if (od_server_pool_total(&route->server_pool) > 0 ||
	    od_client_pool_total(&route->client_pool) > 0)
		goto done;

	if (!od_route_is_dynamic(route) && !route->rule->obsolete)
		goto done;

	/* remove route from route pool */
	assert(pool->count > 0);
	pool->count--;
	od_list_unlink(&route->link);

	od_route_unlock(route);

	/* unref route rule and free route object */
	od_rules_unref(route->rule);
	od_route_free(route);
	return 0;
done:
	od_route_unlock(route);
	return 0;
}

void
od_router_gc(od_router_t *router)
{
	void *argv[] = { &router->route_pool };
	od_router_foreach(router, od_router_gc_cb, argv);
}

void
od_router_stat(od_router_t *router,
               uint64_t prev_time_us,
               int prev_update,
               od_route_pool_stat_cb_t callback,
               void **argv)
{
	od_router_lock(router);
	od_route_pool_stat(&router->route_pool, prev_time_us, prev_update,
	                   callback, argv);
	od_router_unlock(router);
}

od_router_status_t
od_router_route(od_router_t *router, od_config_t *config, od_client_t *client)
{
	kiwi_be_startup_t *startup = &client->startup;

	/* match route */
	assert(startup->database.value_len);
	assert(startup->user.value_len);

	od_router_lock(router);

	/* match latest version of route rule */
	od_rule_t *rule;
	rule = od_rules_forward(&router->rules, startup->database.value,
	                        startup->user.value);
	if (rule == NULL) {
		od_router_unlock(router);
		return OD_ROUTER_ERROR_NOT_FOUND;
	}

	/* force settings required by route */
	od_route_id_t id = {
		.database     = startup->database.value,
		.user         = startup->user.value,
		.database_len = startup->database.value_len,
		.user_len     = startup->user.value_len,
		.physical_rep = false,
		.logical_rep = false
	};
	if (rule->storage_db) {
		id.database = rule->storage_db;
		id.database_len = strlen(rule->storage_db) + 1;
	}
	if (rule->storage_user) {
		id.user = rule->storage_user;
		id.user_len = strlen(rule->storage_user) + 1;
	}
	if (startup->replication.value_len != 0) {
		if (strcmp(startup->replication.value, "database") == 0)
		    id.logical_rep = true;
		else if (!parse_bool(startup->replication.value, &id.physical_rep)) {
            od_router_unlock(router);
            return OD_ROUTER_ERROR_REPLICATION;
        }
	}

	/* match or create dynamic route */
	od_route_t *route;
	route = od_route_pool_match(&router->route_pool, &id, rule);
	if (route == NULL) {
		int is_shared;
		is_shared = od_config_is_multi_workers(config);
		route = od_route_pool_new(&router->route_pool, is_shared, &id, rule);
		if (route == NULL) {
			od_router_unlock(router);
			return OD_ROUTER_ERROR;
		}
	}
	od_rules_ref(rule);

	od_route_lock(route);


    /* ensure route client_max limit */
    if (rule->client_max_set &&
        od_client_pool_total(&route->client_pool) >= rule->client_max) {
        od_rules_unref(rule);
        od_route_unlock(route);
        od_router_unlock(router);
        return OD_ROUTER_ERROR_LIMIT_ROUTE;
    }
	od_router_unlock(router);

	/* add client to route client pool */
	od_client_pool_set(&route->client_pool, client, OD_CLIENT_PENDING);
	client->rule  = rule;
	client->route = route;

	od_route_unlock(route);
	return OD_ROUTER_OK;
}

void
od_router_unroute(od_router_t *router, od_client_t *client)
{
	(void)router;
	/* detach client from route */
	assert(client->route);
	assert(client->server == NULL);

	od_route_t *route = client->route;
	od_route_lock(route);
	od_client_pool_set(&route->client_pool, client, OD_CLIENT_UNDEF);
	client->route = NULL;
	od_route_unlock(route);
}

od_router_status_t
od_router_attach(od_router_t *router, od_config_t *config, od_client_t *client,
                 bool wait_for_idle)
{
	(void)router;
	od_route_t *route = client->route;
	assert(route != NULL);

	od_route_lock(route);

	/* enqueue client (pending -> queue) */
	od_client_pool_set(&route->client_pool, client, OD_CLIENT_QUEUE);

	/* get client server from route server pool */
	bool restart_read = false;
	od_server_t *server;
	int busyloop_sleep = 0;
	int busyloop_retry = 0;
	for (;;)
	{
		server = od_server_pool_next(&route->server_pool, OD_SERVER_IDLE);
		if (server)
			goto attach;

		if (wait_for_idle)
		{
			/* special case, when we are interested only in an idle connection
			 * and do not want to start a new one */
			if (route->server_pool.count_active == 0) {
				od_route_unlock(route);
				return OD_ROUTER_ERROR_TIMEDOUT;
			}
		} else
		{
			/* Maybe start new connection, if pool_size is zero */
			/* Maybe start new connection, if we still have capacity for it */
			if (route->rule->pool_size == 0 || od_server_pool_total(&route->server_pool) < route->rule->pool_size) {
				if (od_atomic_u32_of(&router->servers_routing)	>= (uint32_t) route->rule->storage->server_max_routing) {
					// concurrent server connection in progress.
					od_route_unlock(route);
					machine_sleep(busyloop_sleep);
					busyloop_retry++;
					if (busyloop_retry > 10)
						busyloop_sleep = 1;
					od_route_lock(route);
					continue;
				}
				else {
					// We are allowed to spun new server connection
					break;
				}
			}
		}

		/*
		 * unsubscribe from pending client read events during the time we wait
		 * for an available server
		 */
		restart_read = restart_read || (bool) od_io_read_active(&client->io);
		od_route_unlock(route);

		int rc = od_io_read_stop(&client->io);
		if (rc == -1)
			return OD_ROUTER_ERROR;

		/*
		 * Wait wakeup condition for pool_timeout milliseconds.
		 *
		 * The condition triggered when a server connection
		 * put into idle state by DETACH events.
		 */
		uint32_t timeout = route->rule->pool_timeout;
		if (timeout == 0)
			timeout = UINT32_MAX;
		rc = od_route_wait(route, timeout);
		if (rc == -1)
			return OD_ROUTER_ERROR_TIMEDOUT;

		od_route_lock(route);
	}

	od_route_unlock(route);

	/* create new server object */
	server = od_server_allocate();
	if (server == NULL)
		return OD_ROUTER_ERROR;
	od_id_generate(&server->id, "s");
	server->global = client->global;
	server->route  = route;

	od_route_lock(route);

attach:
	od_server_pool_set(&route->server_pool, server, OD_SERVER_ACTIVE);
	od_client_pool_set(&route->client_pool, client, OD_CLIENT_ACTIVE);

	client->server     = server;
	server->client     = client;
	server->idle_time  = 0;
	server->key_client = client->key;

	od_route_unlock(route);

	/* attach server io to clients machine context */
	if (server->io.io && od_config_is_multi_workers(config))
		od_io_attach(&server->io);

	/* maybe restore read events subscription */
	if (restart_read)
		od_io_read_start(&client->io);

	return OD_ROUTER_OK;
}

void
od_router_detach(od_router_t *router, od_config_t *config, od_client_t *client)
{
	(void)router;
	od_route_t *route = client->route;
	assert(route != NULL);

	/* detach from current machine event loop */
	od_server_t *server = client->server;
	if (od_config_is_multi_workers(config))
		od_io_detach(&server->io);

	od_route_lock(route);

	client->server = NULL;
	server->client = NULL;
	od_server_pool_set(&route->server_pool, server, OD_SERVER_IDLE);
	od_client_pool_set(&route->client_pool, client, OD_CLIENT_PENDING);

	int signal = route->client_pool.count_queue > 0;
	od_route_unlock(route);

	/* notify waiters */
	if (signal)
		od_route_signal(route);
}

void
od_router_close(od_router_t *router, od_client_t *client)
{
	(void)router;
	od_route_t *route = client->route;
	assert(route != NULL);

	od_server_t *server = client->server;
	od_backend_close_connection(server);

	od_route_lock(route);

	od_client_pool_set(&route->client_pool, client, OD_CLIENT_PENDING);
	od_server_pool_set(&route->server_pool, server, OD_SERVER_UNDEF);
	client->server = NULL;
	server->client = NULL;
	server->route  = NULL;

	od_route_unlock(route);

	assert(server->io.io == NULL);
	od_server_free(server);
}

static inline int
od_router_cancel_cmp(od_server_t *server, void **argv)
{
	return kiwi_key_cmp(&server->key_client, argv[0]);
}

static inline int
od_router_cancel_cb(od_route_t *route, void **argv)
{
	od_route_lock(route);

	od_server_t *server;
	server = od_server_pool_foreach(&route->server_pool, OD_SERVER_ACTIVE,
	                                od_router_cancel_cmp, argv);
	if (server)
	{
		od_router_cancel_t *cancel = argv[1];
		cancel->id     = server->id;
		cancel->key    = server->key;
		cancel->storage = od_rules_storage_copy(route->rule->storage);
		od_route_unlock(route);
		if (cancel->storage == NULL)
			return -1;
		return 1;
	}

	od_route_unlock(route);
	return 0;
}

od_router_status_t
od_router_cancel(od_router_t *router, kiwi_key_t *key, od_router_cancel_t *cancel)
{
	/* match server by client forged key */
	void *argv[] = { key, cancel };
	int rc;
	rc = od_router_foreach(router, od_router_cancel_cb, argv);
	if (rc <= 0)
		return OD_ROUTER_ERROR_NOT_FOUND;
	return OD_ROUTER_OK;
}

static inline int
od_router_kill_cb(od_route_t *route, void **argv)
{
	od_route_lock(route);
	od_route_kill_client(route, argv[0]);
	od_route_unlock(route);
	return 0;
}

void
od_router_kill(od_router_t *router, od_id_t *id)
{
	void *argv[] = { id };
	od_router_foreach(router, od_router_kill_cb, argv);
}
