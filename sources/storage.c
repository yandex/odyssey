
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>

#include <status.h>
#include <storage.h>
#include <global.h>
#include <instance.h>
#include <route.h>
#include <backend.h>
#include <query.h>
#include <attach.h>
#include <router.h>
#include <auth_query.h>
#include <internal_client.h>

void od_storage_endpoint_status_init(od_storage_endpoint_status_t *status)
{
	status->last_update_time_ms = 0ULL;
	status->alive = 1;
	status->is_read_write = true;
	status->repl_time_sec = 0;
	pthread_spin_init(&status->values_lock, PTHREAD_PROCESS_PRIVATE);
}

void od_storage_endpoint_status_destroy(od_storage_endpoint_status_t *status)
{
	pthread_spin_destroy(&status->values_lock);
}

bool od_storage_endpoint_status_is_outdated(
	od_storage_endpoint_status_t *status, uint64_t recheck_interval)
{
	pthread_spin_lock(&status->values_lock);

	uint64_t last_update_time_ms = status->last_update_time_ms;

	pthread_spin_unlock(&status->values_lock);

	return (machine_time_ms() - last_update_time_ms) > recheck_interval;
}

void od_storage_endpoint_status_get(od_storage_endpoint_status_t *status,
				    od_storage_endpoint_status_t *out)
{
	pthread_spin_lock(&status->values_lock);

	out->last_update_time_ms = status->last_update_time_ms;
	out->is_read_write = status->is_read_write;
	out->alive = status->alive;
	out->repl_time_sec = status->repl_time_sec;

	pthread_spin_unlock(&status->values_lock);
}

void od_storage_endpoint_status_set(od_storage_endpoint_status_t *status,
				    const od_storage_endpoint_status_t *value)
{
	pthread_spin_lock(&status->values_lock);

	status->last_update_time_ms = value->last_update_time_ms;
	status->is_read_write = value->is_read_write;
	status->alive = value->alive;
	status->repl_time_sec = value->repl_time_sec;

	pthread_spin_unlock(&status->values_lock);
}

void od_storage_endpoint_status_set_dead(od_storage_endpoint_status_t *status)
{
	od_storage_endpoint_status_t endp_status;
	od_storage_endpoint_status_init(&endp_status);
	endp_status.alive = 0;
	endp_status.is_read_write = 0;
	endp_status.last_update_time_ms = machine_time_ms();
	endp_status.repl_time_sec = 0;

	od_storage_endpoint_status_set(status, &endp_status);
}

od_storage_watchdog_t *od_storage_watchdog_allocate(od_global_t *global)
{
	od_storage_watchdog_t *watchdog;
	watchdog = od_malloc(sizeof(od_storage_watchdog_t));
	if (watchdog == NULL) {
		return NULL;
	}
	memset(watchdog, 0, sizeof(od_storage_watchdog_t));
	watchdog->global = global;
	watchdog->is_finished = machine_wait_flag_create();
	if (watchdog->is_finished == NULL) {
		od_free(watchdog);
		return NULL;
	}

	watchdog->online = machine_wait_flag_create();
	if (watchdog->online == NULL) {
		machine_wait_flag_destroy(watchdog->is_finished);
		od_free(watchdog);
		return NULL;
	}

	return watchdog;
}

static inline int
od_storage_watchdog_set_offline(od_storage_watchdog_t *watchdog)
{
	machine_wait_flag_set(watchdog->online);
	return OK_RESPONSE;
}

void od_storage_watchdog_soft_exit(od_storage_watchdog_t *watchdog)
{
	od_storage_watchdog_set_offline(watchdog);
	/*
	 * can't wait the coroutine to finish,
	 * it can be run on other thread
	 */
	machine_wait_flag_wait(watchdog->is_finished, UINT32_MAX);
	od_storage_watchdog_free(watchdog);
}

int od_storage_watchdog_free(od_storage_watchdog_t *watchdog)
{
	if (watchdog == NULL) {
		return NOT_OK_RESPONSE;
	}

	if (watchdog->query) {
		od_free(watchdog->query);
	}

	machine_wait_flag_destroy(watchdog->is_finished);
	machine_wait_flag_destroy(watchdog->online);

	od_free(watchdog);
	return OK_RESPONSE;
}

od_rule_storage_t *od_rules_storage_allocate(void)
{
	/* Allocate and force defaults */
	od_rule_storage_t *storage =
		(od_rule_storage_t *)od_malloc(sizeof(od_rule_storage_t));
	if (storage == NULL) {
		return NULL;
	}
	memset(storage, 0, sizeof(*storage));
	storage->tls_opts = od_tls_opts_alloc();
	if (storage->tls_opts == NULL) {
		od_free(storage);
		return NULL;
	}
	storage->endpoints_status_poll_interval_ms = 1000;
	od_storage_balancing_init(&storage->balancing);
	atomic_init(&storage->rr_counter, 0);

	od_list_init(&storage->link);
	return storage;
}

void od_rules_storage_free(od_rule_storage_t *storage)
{
	if (storage->name) {
		od_free(storage->name);
	}
	if (storage->type) {
		od_free(storage->type);
	}
	if (storage->host) {
		od_free(storage->host);
	}

	if (storage->tls_opts) {
		od_tls_opts_free(storage->tls_opts);
	}

	if (storage->endpoints_count) {
		for (size_t i = 0; i < storage->endpoints_count; ++i) {
			od_storage_endpoint_status_destroy(
				&storage->endpoints[i].status);
			od_address_destroy(&storage->endpoints[i].address);
		}

		od_free(storage->endpoints);
	}

	od_storage_balancing_destroy(&storage->balancing);

	od_list_unlink(&storage->link);
	od_free(storage);
}

od_rule_storage_t *od_rules_storage_copy(od_rule_storage_t *storage)
{
	od_rule_storage_t *copy;
	copy = od_rules_storage_allocate();
	if (copy == NULL) {
		return NULL;
	}
	copy->storage_type = storage->storage_type;
	copy->name = od_strdup(storage->name);
	copy->server_max_routing = storage->server_max_routing;
	if (copy->name == NULL) {
		goto error;
	}
	copy->type = od_strdup(storage->type);
	if (copy->type == NULL) {
		goto error;
	}
	if (storage->host) {
		copy->host = od_strdup(storage->host);
		if (copy->host == NULL) {
			goto error;
		}
	}
	copy->port = storage->port;
	copy->tls_opts->tls_mode = storage->tls_opts->tls_mode;
	if (storage->tls_opts->tls) {
		copy->tls_opts->tls = od_strdup(storage->tls_opts->tls);
		if (copy->tls_opts->tls == NULL) {
			goto error;
		}
	}
	if (storage->tls_opts->tls_ca_file) {
		copy->tls_opts->tls_ca_file =
			od_strdup(storage->tls_opts->tls_ca_file);
		if (copy->tls_opts->tls_ca_file == NULL) {
			goto error;
		}
	}
	if (storage->tls_opts->tls_key_file) {
		copy->tls_opts->tls_key_file =
			od_strdup(storage->tls_opts->tls_key_file);
		if (copy->tls_opts->tls_key_file == NULL) {
			goto error;
		}
	}
	if (storage->tls_opts->tls_cert_file) {
		copy->tls_opts->tls_cert_file =
			od_strdup(storage->tls_opts->tls_cert_file);
		if (copy->tls_opts->tls_cert_file == NULL) {
			goto error;
		}
	}
	if (storage->tls_opts->tls_protocols) {
		copy->tls_opts->tls_protocols =
			od_strdup(storage->tls_opts->tls_protocols);
		if (copy->tls_opts->tls_protocols == NULL) {
			goto error;
		}
	}

	if (storage->endpoints_count) {
		copy->endpoints_count = storage->endpoints_count;
		copy->endpoints = od_malloc(sizeof(od_storage_endpoint_t) *
					    copy->endpoints_count);
		if (copy->endpoints == NULL) {
			goto error;
		}

		for (size_t i = 0; i < copy->endpoints_count; ++i) {
			od_address_init(&copy->endpoints[i].address);

			if (od_address_copy(&copy->endpoints[i].address,
					    &storage->endpoints[i].address) !=
			    OK_RESPONSE) {
				goto error;
			}
			od_storage_endpoint_status_init(
				&copy->endpoints[i].status);
		}
	}

	od_storage_balancing_copy(&copy->balancing, &storage->balancing);

	copy->endpoints_status_poll_interval_ms =
		storage->endpoints_status_poll_interval_ms;

	/* storage auth cache not copied */

	return copy;
error:
	od_rules_storage_free(copy);
	return NULL;
}

od_storage_endpoint_t *od_storage_find_endpoint(od_rule_storage_t *storage,
						const od_address_t *address)
{
	for (size_t i = 0; i < storage->endpoints_count; ++i) {
		od_storage_endpoint_t *e = &storage->endpoints[i];

		if (od_address_cmp(&e->address, address) == 0) {
			return e;
		}
	}

	return NULL;
}

static inline int od_router_update_heartbeat_cb(od_route_t *route, void **argv)
{
	od_route_lock(route);
	route->last_heartbeat = *(int *)argv[0];
	od_route_unlock(route);
	return 0;
}

static inline od_client_t *
od_storage_watchdog_prepare_client(od_storage_watchdog_t *watchdog)
{
	od_global_t *global = watchdog->global;
	od_router_t *router = global->router;
	od_instance_t *instance = global->instance;

	od_client_t *watchdog_client;
	watchdog_client =
		od_client_allocate_internal(global, "storage-watchdog");
	if (watchdog_client == NULL) {
		od_error(&instance->logger, "watchdog", NULL, NULL,
			 "route storage watchdog failed to allocate client");
		return NULL;
	}

	watchdog_client->is_watchdog = true;
	watchdog_client->global = global;
	watchdog_client->type = OD_POOL_CLIENT_INTERNAL;
	od_id_generate(&watchdog_client->id, "a");

	/* set storage user and database */
	kiwi_var_set(&watchdog_client->startup.user, KIWI_VAR_UNDEF,
		     watchdog->route_usr, strlen(watchdog->route_usr) + 1);

	kiwi_var_set(&watchdog_client->startup.database, KIWI_VAR_UNDEF,
		     watchdog->route_db, strlen(watchdog->route_db) + 1);

	od_router_status_t status;
	status = od_router_route(router, watchdog_client);
	od_debug(&instance->logger, "watchdog", watchdog_client, NULL,
		 "routing to internal wd route status: %s",
		 od_router_status_to_str(status));

	if (status != OD_ROUTER_OK) {
		od_error(&instance->logger, "watchdog", watchdog_client, NULL,
			 "route storage watchdog failed: %s",
			 od_router_status_to_str(status));

		od_client_free_extended(watchdog_client);

		return NULL;
	}

	return watchdog_client;
}

static inline void
od_storage_watchdog_close_client(od_storage_watchdog_t *watchdog,
				 od_client_t *watchdog_client)
{
	od_global_t *global = watchdog->global;
	od_router_t *router = global->router;

	od_router_close(router, watchdog_client);
	od_router_unroute(router, watchdog_client);
	od_client_free_extended(watchdog_client);
}

static inline od_client_t *
od_storage_create_and_connect_watchdog_client(od_storage_watchdog_t *watchdog,
					      od_storage_endpoint_t *endp)
{
	od_global_t *global = watchdog->global;
	od_router_t *router = global->router;
	od_instance_t *instance = global->instance;

	od_client_t *client;
	client = od_storage_watchdog_prepare_client(watchdog);
	if (client == NULL) {
		od_error(&instance->logger, "watchdog", NULL, NULL,
			 "route storage watchdog failed to prepare client");

		return NULL;
	}

	od_frontend_status_t status;
	status = od_attach_extended_endpoint(instance, "watchdog", router,
					     client, endp);
	if (status != OD_OK) {
		od_error(&instance->logger, "watchdog", client, NULL,
			 "attach failed with status = %d (%s)", status,
			 od_frontend_status_to_str(status));

		od_router_unroute(router, client);
		od_client_free_extended(client);

		return NULL;
	}

	return client;
}

static inline void od_storage_update_route_last_heartbeats(
	od_instance_t *instance, od_client_t *watchdog_client,
	od_server_t *server, od_router_t *router, int last_heartbeat)
{
	od_log(&instance->logger, "watchdog", watchdog_client, server,
	       "send heartbeat arenda update to routes with value %d",
	       last_heartbeat);

	void *argv[] = { &last_heartbeat };

	od_router_foreach(router, od_router_update_heartbeat_cb, argv);
}

static inline void watchdog_update_endpoint(od_storage_watchdog_t *watchdog,
					    od_storage_endpoint_t *endp)
{
	od_global_t *global = watchdog->global;
	od_router_t *router = global->router;
	od_instance_t *instance = global->instance;

	od_client_t *client;
	client = od_storage_create_and_connect_watchdog_client(watchdog, endp);
	if (client == NULL) {
		return;
	}

	od_server_t *server = client->server;

	/* try to update, ignore errors - cant do nothing with it */
	int rc = od_backend_update_endpoint_status(
		instance, client, server, "watchdog", watchdog->query, endp);

	if (rc == 0) {
		od_storage_endpoint_status_t status;
		od_storage_endpoint_status_get(&endp->status, &status);

		od_storage_update_route_last_heartbeats(
			instance, client, server, router,
			(int)status.repl_time_sec);
	}

	od_storage_watchdog_close_client(watchdog, client);
}

static inline void
od_storage_watchdog_do_polling_step(od_storage_watchdog_t *watchdog)
{
	od_rule_storage_t *storage = watchdog->storage;

	for (size_t i = 0; i < storage->endpoints_count; ++i) {
		od_storage_endpoint_t *endp = &storage->endpoints[i];

		watchdog_update_endpoint(watchdog, endp);
	}
}

static inline void
od_storage_watchdog_do_polling_loop(od_storage_watchdog_t *watchdog)
{
	od_instance_t *instance = od_global_get_instance();
	od_rule_storage_t *storage = watchdog->storage;

	uint64_t poll_ms = storage->endpoints_status_poll_interval_ms;

	/*
	 * increase the probability of using
	 * the status obtained through watchdog
	 */
	poll_ms = poll_ms * 3 / 4;
	if (poll_ms == 0) {
		poll_ms = storage->endpoints_status_poll_interval_ms;
	}

	while (1) {
		int rc = machine_wait_flag_wait(watchdog->online, poll_ms);
		if (rc == 0) {
			od_log(&instance->logger, "watchdog", NULL, NULL,
			       "online flag is set, exiting from watchdog for %s",
			       watchdog->storage->name);
			break;
		}

		if (rc == -1 && machine_errno() != ETIMEDOUT) {
			od_error(&instance->logger, "watchdog", NULL, NULL,
				 "can't wait online flag: %s (%d)",
				 strerror(machine_errno()), machine_errno());
			break;
		}

		od_storage_watchdog_do_polling_step(watchdog);
	}
}

void od_storage_watchdog_watch(void *arg)
{
	od_storage_watchdog_t *watchdog = (od_storage_watchdog_t *)arg;
	od_global_t *global = watchdog->global;
	od_instance_t *instance = global->instance;

	od_debug(&instance->logger, "watchdog", NULL, NULL,
		 "start watchdog for storage '%s'", watchdog->storage->name);

	od_storage_watchdog_do_polling_loop(watchdog);

	od_log(&instance->logger, "watchdog", NULL, NULL,
	       "finishing watchdog for storage '%s'", watchdog->storage->name);
	machine_wait_flag_set(watchdog->is_finished);
}

int od_storage_parse_endpoints(const char *host_str,
			       od_storage_endpoint_t **out, size_t *count)
{
	od_address_t *addrs = NULL;

	if (od_parse_addresses(host_str, &addrs, count) != OK_RESPONSE) {
		return NOT_OK_RESPONSE;
	}

	size_t c = *count;

	if (c > OD_STORAGE_MAX_ENDPOINTS) {
		od_free(addrs);
		return NOT_OK_RESPONSE;
	}

	od_storage_endpoint_t *result =
		od_malloc(c * sizeof(od_storage_endpoint_t));
	if (result == NULL) {
		od_free(addrs);
		return NOT_OK_RESPONSE;
	}

	for (size_t i = 0; i < c; ++i) {
		od_address_t *result_addr = &result[i].address;
		od_address_init(result_addr);
		od_address_move(result_addr, &addrs[i]);
	}

	od_free(addrs);

	*out = result;
	/* count is set in od_parse_addresses */

	return OK_RESPONSE;
}
