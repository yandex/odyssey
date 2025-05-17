
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

bool od_storage_endpoint_equal(od_storage_endpoint_t *a,
			       od_storage_endpoint_t *b)
{
	if (a->host == NULL) {
		return b->host == NULL;
	}

	if (b->host == NULL) {
		return false; /* a->host != NULL */
	}

	return a->port == b->port && strcmp(a->host, b->host) == 0;
}

bool od_storage_endpoint_status_is_outdated(od_storage_endpoint_t *endpoint)
{
	uint64_t ms_since_last_update =
		machine_time_ms() - endpoint->status.last_update_time_ms;
	return ms_since_last_update >= 2 * 1000 /* 2 secs */;
}

od_storage_watchdog_t *od_storage_watchdog_allocate(od_global_t *global)
{
	od_storage_watchdog_t *watchdog;
	watchdog = malloc(sizeof(od_storage_watchdog_t));
	if (watchdog == NULL) {
		return NULL;
	}
	memset(watchdog, 0, sizeof(od_storage_watchdog_t));
	watchdog->global = global;
	watchdog->online = 1;
	watchdog->route_usr = NULL;
	watchdog->route_db = NULL;
	od_atomic_u64_set(&watchdog->finished, 0ULL);
	pthread_mutex_init(&watchdog->mu, NULL);

	return watchdog;
}

static inline int od_storage_watchdog_is_online(od_storage_watchdog_t *watchdog)
{
	int ret;
	pthread_mutex_lock(&watchdog->mu);
	ret = watchdog->online;
	pthread_mutex_unlock(&watchdog->mu);
	return ret;
}

static inline int
od_storage_watchdog_set_offline(od_storage_watchdog_t *watchdog)
{
	pthread_mutex_lock(&watchdog->mu);
	watchdog->online = 0;
	pthread_mutex_unlock(&watchdog->mu);
	return OK_RESPONSE;
}

static inline void
od_storage_watchdog_soft_exit(od_storage_watchdog_t *watchdog)
{
	od_storage_watchdog_set_offline(watchdog);
	while (od_atomic_u64_of(&watchdog->finished) != 1ULL) {
		machine_sleep(300);
	}
	od_storage_watchdog_free(watchdog);
}

int od_storage_watchdog_free(od_storage_watchdog_t *watchdog)
{
	if (watchdog == NULL) {
		return NOT_OK_RESPONSE;
	}

	if (watchdog->query) {
		free(watchdog->query);
	}

	free(watchdog->route_db);
	free(watchdog->route_usr);

	pthread_mutex_destroy(&watchdog->mu);

	free(watchdog);
	return OK_RESPONSE;
}

od_rule_storage_t *od_rules_storage_allocate(void)
{
	/* Allocate and force defaults */
	od_rule_storage_t *storage =
		(od_rule_storage_t *)malloc(sizeof(od_rule_storage_t));
	if (storage == NULL)
		return NULL;
	memset(storage, 0, sizeof(*storage));
	storage->tls_opts = od_tls_opts_alloc();
	if (storage->tls_opts == NULL) {
		free(storage);
		return NULL;
	}
	storage->target_session_attrs = OD_TARGET_SESSION_ATTRS_ANY;
	storage->rr_counter = 0;

#define OD_STORAGE_DEFAULT_HASHMAP_SZ 420u

	storage->acache = od_hashmap_create(OD_STORAGE_DEFAULT_HASHMAP_SZ);

	od_list_init(&storage->link);
	return storage;
}

void od_rules_storage_free(od_rule_storage_t *storage)
{
	if (storage->watchdog) {
		od_storage_watchdog_soft_exit(storage->watchdog);
	}

	if (storage->name)
		free(storage->name);
	if (storage->type)
		free(storage->type);
	if (storage->host)
		free(storage->host);

	if (storage->tls_opts) {
		od_tls_opts_free(storage->tls_opts);
	}

	if (storage->endpoints_count) {
		for (size_t i = 0; i < storage->endpoints_count; ++i) {
			free(storage->endpoints[i].host);
		}

		free(storage->endpoints);
	}

	if (storage->acache) {
		od_hashmap_free(storage->acache);
	}

	od_list_unlink(&storage->link);
	free(storage);
}

od_rule_storage_t *od_rules_storage_copy(od_rule_storage_t *storage)
{
	od_rule_storage_t *copy;
	copy = od_rules_storage_allocate();
	if (copy == NULL)
		return NULL;
	copy->storage_type = storage->storage_type;
	copy->name = strdup(storage->name);
	copy->server_max_routing = storage->server_max_routing;
	if (copy->name == NULL)
		goto error;
	copy->type = strdup(storage->type);
	if (copy->type == NULL)
		goto error;
	if (storage->host) {
		copy->host = strdup(storage->host);
		if (copy->host == NULL)
			goto error;
	}
	copy->port = storage->port;
	copy->tls_opts->tls_mode = storage->tls_opts->tls_mode;
	if (storage->tls_opts->tls) {
		copy->tls_opts->tls = strdup(storage->tls_opts->tls);
		if (copy->tls_opts->tls == NULL)
			goto error;
	}
	if (storage->tls_opts->tls_ca_file) {
		copy->tls_opts->tls_ca_file =
			strdup(storage->tls_opts->tls_ca_file);
		if (copy->tls_opts->tls_ca_file == NULL)
			goto error;
	}
	if (storage->tls_opts->tls_key_file) {
		copy->tls_opts->tls_key_file =
			strdup(storage->tls_opts->tls_key_file);
		if (copy->tls_opts->tls_key_file == NULL)
			goto error;
	}
	if (storage->tls_opts->tls_cert_file) {
		copy->tls_opts->tls_cert_file =
			strdup(storage->tls_opts->tls_cert_file);
		if (copy->tls_opts->tls_cert_file == NULL)
			goto error;
	}
	if (storage->tls_opts->tls_protocols) {
		copy->tls_opts->tls_protocols =
			strdup(storage->tls_opts->tls_protocols);
		if (copy->tls_opts->tls_protocols == NULL)
			goto error;
	}

	if (storage->endpoints_count) {
		copy->endpoints_count = storage->endpoints_count;
		copy->endpoints = malloc(sizeof(od_storage_endpoint_t) *
					 copy->endpoints_count);
		if (copy->endpoints == NULL) {
			goto error;
		}

		for (size_t i = 0; i < copy->endpoints_count; ++i) {
			copy->endpoints[i].host =
				strdup(storage->endpoints[i].host);
			if (copy->endpoints[i].host == NULL) {
				goto error;
			}
			copy->endpoints[i].port = storage->endpoints[i].port;
			memcpy(&copy->endpoints[i].status,
			       &storage->endpoints[i].status,
			       sizeof(od_storage_endpoint_status_t));
		}
	}

	/* storage auth cache not copied */

	copy->target_session_attrs = storage->target_session_attrs;

	return copy;
error:
	od_rules_storage_free(copy);
	return NULL;
}

bool od_rules_storage_equal_names(od_rule_storage_t *a, od_rule_storage_t *b)
{
	/* Names can not be NULL */
	return strcmp(a->name, b->name) == 0;
}

static inline int strtol_safe(const char *s, int len)
{
	const int buff_len = 32;
	char buff[buff_len];
	memset(buff, 0, sizeof(buff));
	memcpy(buff, s, len);

	return strtol(buff, NULL, 0);
}

static inline int od_storage_watchdog_parse_lag_from_datarow(machine_msg_t *msg,
							     int *repl_lag)
{
	char *pos = (char *)machine_msg_data(msg) + 1;
	uint32_t pos_size = machine_msg_size(msg) - 1;

	/* size */
	uint32_t size;
	int rc;
	rc = kiwi_read32(&size, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1))
		goto error;
	/* count */
	uint16_t count;
	rc = kiwi_read16(&count, &pos, &pos_size);

	if (kiwi_unlikely(rc == -1))
		goto error;

	if (count != 1)
		goto error;

	uint32_t lag_len;
	rc = kiwi_read32(&lag_len, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1)) {
		goto error;
	}

	*repl_lag = strtol_safe(pos, (int)lag_len);

	return OK_RESPONSE;
error:
	return NOT_OK_RESPONSE;
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

		od_client_free(watchdog_client);

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
	od_client_free(watchdog_client);
}

static inline od_client_t *
od_storage_create_and_connect_watchdog_client(od_storage_watchdog_t *watchdog)
{
	od_global_t *global = watchdog->global;
	od_router_t *router = global->router;
	od_instance_t *instance = global->instance;

	od_client_t *watchdog_client;
	watchdog_client = od_storage_watchdog_prepare_client(watchdog);
	if (watchdog_client == NULL) {
		od_error(&instance->logger, "watchdog", NULL, NULL,
			 "route storage watchdog failed to prepare client");

		return NULL;
	}

	od_router_status_t status;
	status = od_router_attach(router, watchdog_client, false);
	od_debug(&instance->logger, "watchdog", watchdog_client, NULL,
		 "attaching wd client to backend connection status: %s",
		 od_router_status_to_str(status));
	if (status != OD_ROUTER_OK) {
		od_error(&instance->logger, "watchdog", watchdog_client, NULL,
			 "can't attach wd client to backend connection: %s",
			 od_router_status_to_str(status));

		od_client_free(watchdog_client);

		return NULL;
	}

	od_server_t *server;
	server = watchdog_client->server;
	od_debug(&instance->logger, "watchdog", watchdog_client, server,
		 "attached to server %s%.*s", server->id.id_prefix,
		 (int)sizeof(server->id.id), server->id.id);

	/* connect to server, if necessary */
	if (server->io.io == NULL) {
		int rc;
		rc = od_backend_connect(server, "watchdog", NULL,
					watchdog_client);
		if (rc == NOT_OK_RESPONSE) {
			od_debug(&instance->logger, "watchdog", watchdog_client,
				 server, "backend connect failed");

			od_storage_watchdog_close_client(watchdog,
							 watchdog_client);

			return NULL;
		}
	}

	return watchdog_client;
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

static inline int
od_storage_watchdog_do_lag_polling(od_storage_watchdog_t *watchdog)
{
	od_global_t *global;
	global = watchdog->global;

	od_router_t *router;
	router = global->router;

	od_instance_t *instance;
	instance = global->instance;

	od_client_t *watchdog_client;
	watchdog_client =
		od_storage_create_and_connect_watchdog_client(watchdog);
	if (watchdog_client == NULL) {
		od_error(&instance->logger, "watchdog", watchdog_client, NULL,
			 "failed to create lag polling watchdog client");
		return NOT_OK_RESPONSE;
	}

	od_server_t *server;
	server = watchdog_client->server;

	machine_msg_t *msg;
	msg = od_query_do(server, "watchdog", watchdog->query, NULL);
	if (msg == NULL) {
		od_error(&instance->logger, "watchdog", watchdog_client, server,
			 "receive msg failed, closing backend connection");

		od_storage_watchdog_close_client(watchdog, watchdog_client);
		return NOT_OK_RESPONSE;
	}

	int last_heartbeat;
	if (od_storage_watchdog_parse_lag_from_datarow(msg, &last_heartbeat) ==
	    0) {
		od_storage_update_route_last_heartbeats(instance,
							watchdog_client, server,
							router, last_heartbeat);
	} else {
		od_error(&instance->logger, "watchdog", watchdog_client, server,
			 "can't parse lag");
	}

	machine_msg_free(msg);
	od_storage_watchdog_close_client(watchdog, watchdog_client);

	return OK_RESPONSE;
}

static inline od_client_t *
od_storage_watchdog_update_tsa_client_create(od_storage_watchdog_t *watchdog,
					     od_storage_endpoint_t *endpoint,
					     size_t endpoint_idx)
{
	od_global_t *global = watchdog->global;
	od_router_t *router = global->router;
	od_instance_t *instance = global->instance;

	od_client_t *tsa_update_client;
	tsa_update_client = od_storage_watchdog_prepare_client(watchdog);
	if (tsa_update_client == NULL) {
		od_error(&instance->logger, "watchdog", NULL, NULL,
			 "route storage watchdog failed to prepare client");

		return NULL;
	}

	od_router_status_t status;
	status = od_router_attach(router, tsa_update_client, false);
	od_debug(&instance->logger, "watchdog", tsa_update_client, NULL,
		 "attaching wd client to backend connection status: %s",
		 od_router_status_to_str(status));
	if (status != OD_ROUTER_OK) {
		od_error(&instance->logger, "watchdog", tsa_update_client, NULL,
			 "can't attach wd client to backend connection: %s",
			 od_router_status_to_str(status));

		od_client_free(tsa_update_client);

		return NULL;
	}

	od_server_t *server;
	server = tsa_update_client->server;
	od_debug(&instance->logger, "watchdog", tsa_update_client, server,
		 "attached to server %s%.*s", server->id.id_prefix,
		 (int)sizeof(server->id.id), server->id.id);

	/* if connected to another endpoint, disconnect */
	if (server->io.io != NULL &&
	    server->endpoint_selector != endpoint_idx) {
		od_backend_close_connection(server);
	}

	/* wasn't connected or was connected to another endpoint */
	if (server->io.io == NULL) {
		int rc;
		rc = od_backend_connect_specific_endpoint(
			server, "watchdog", NULL, tsa_update_client, endpoint,
			endpoint_idx);
		if (rc == NOT_OK_RESPONSE) {
			od_debug(
				&instance->logger, "watchdog",
				tsa_update_client, server,
				"backend connect failed to specific endpoint %s:%d",
				endpoint->host, endpoint->port);

			od_storage_watchdog_close_client(watchdog,
							 tsa_update_client);

			return NULL;
		}
	}

	return tsa_update_client;
}

static inline int
od_storage_parse_rw_check_response(machine_msg_t *msg,
				   od_target_session_attrs_t *out)
{
	char *pos = (char *)machine_msg_data(msg) + 1;
	uint32_t pos_size = machine_msg_size(msg) - 1;

	/* size */
	uint32_t size;
	int rc;
	rc = kiwi_read32(&size, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1))
		goto error;
	/* count */
	uint16_t count;
	rc = kiwi_read16(&count, &pos, &pos_size);

	if (kiwi_unlikely(rc == -1))
		goto error;

	if (count != 1)
		goto error;

	/* (not used) */
	uint32_t resp_len;
	rc = kiwi_read32(&resp_len, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1)) {
		goto error;
	}

	/* we expect exactly one row */
	if (resp_len != 1) {
		return NOT_OK_RESPONSE;
	}
	/* pg is in recovery false means db is open for write */
	if (pos[0] == 'f') {
		*out = OD_TARGET_SESSION_ATTRS_RW;
	} else {
		*out = OD_TARGET_SESSION_ATTRS_RO;
	}

	return OK_RESPONSE;

	/* fallthrough to error */
error:
	return NOT_OK_RESPONSE;
}

typedef struct {
	od_storage_watchdog_t *watchdog;
	od_storage_endpoint_t *endpoint;
	size_t endpoint_idx;
	machine_wait_group_t *wg;
	int status;
} tsa_endpoint_update_arg_t;

typedef struct {
	od_rule_storage_t *target_storage;
	od_storage_endpoint_t *target_endpoint;
	od_storage_endpoint_status_t status;
} storage_endpoint_update_arg_t;

static inline int od_storage_watchdog_update_tsa_cb(od_rule_t *rule,
						    void **argv)
{
	storage_endpoint_update_arg_t *arg;
	arg = (storage_endpoint_update_arg_t *)argv[0];

	od_rule_storage_t *storage = rule->storage;
	if (od_unlikely(storage == NULL)) {
		return 0;
	}

	if (!od_rules_storage_equal_names(storage, arg->target_storage)) {
		return 0;
	}

	for (size_t i = 0; i < storage->endpoints_count; ++i) {
		od_storage_endpoint_t *ep = &storage->endpoints[i];

		if (od_storage_endpoint_equal(ep, arg->target_endpoint)) {
			memcpy(&ep->status, &arg->status,
			       sizeof(od_storage_endpoint_status_t));
		}
	}

	return 0;
}

static inline int od_storage_watchdog_get_tsa(od_instance_t *instance,
					      od_storage_endpoint_t *endpoint,
					      od_client_t *client,
					      od_server_t *server,
					      od_target_session_attrs_t *out)
{
	machine_msg_t *msg;
	msg = od_query_do(server, "watchdog", "SELECT pg_is_in_recovery()",
			  NULL);
	if (msg == NULL) {
		od_error(&instance->logger, "watchdog", client, server,
			 "can't execute pg_is_in_recovery() query");
		return NOT_OK_RESPONSE;
	}

	if (od_storage_parse_rw_check_response(msg, out) != OK_RESPONSE) {
		od_error(&instance->logger, "watchdog", client, server,
			 "can't parse pg_is_in_recovery() result");
		return NOT_OK_RESPONSE;
	}

	od_log(&instance->logger, "watchdog", client, server,
	       "updated mode for %s:%d: %s", endpoint->host, endpoint->port,
	       od_target_session_attrs_to_pg_mode_str(*out));

	return OK_RESPONSE;
}

static inline void od_storage_watchdog_update_tsa_on_endpoint(void *arg)
{
	tsa_endpoint_update_arg_t *targ = arg;
	od_storage_watchdog_t *watchdog = targ->watchdog;
	od_storage_endpoint_t *endpoint = targ->endpoint;
	size_t endpoint_idx = targ->endpoint_idx;
	machine_wait_group_t *wg = targ->wg;

	od_global_t *global = watchdog->global;
	od_instance_t *instance = global->instance;
	od_router_t *router = global->router;

	od_client_t *tsa_update_client;
	tsa_update_client = od_storage_watchdog_update_tsa_client_create(
		watchdog, endpoint, endpoint_idx);
	if (tsa_update_client == NULL) {
		targ->status = NOT_OK_RESPONSE;
		machine_wait_group_done(wg);
		return;
	}

	od_server_t *server;
	server = tsa_update_client->server;

	storage_endpoint_update_arg_t update_arg;
	memset(&update_arg, 0, sizeof(storage_endpoint_update_arg_t));

	int rc;
	rc = od_storage_watchdog_get_tsa(instance, endpoint, tsa_update_client,
					 server, &update_arg.status.tsa);
	if (rc == OK_RESPONSE) {
		update_arg.status.alive = OD_STORAGE_ENDPOINT_STATUS_ALIVE;
	} else {
		update_arg.status.alive = OD_STORAGE_ENDPOINT_STATUS_DOWN;
	}

	update_arg.status.last_update_time_ms = machine_time_ms();
	update_arg.target_endpoint = endpoint;
	update_arg.target_storage = watchdog->storage;
	targ->status = rc;

	void *argv[] = { &update_arg };

	od_router_rules_foreach(router, od_storage_watchdog_update_tsa_cb,
				argv);

	od_storage_watchdog_close_client(watchdog, tsa_update_client);
	machine_wait_group_done(wg);
}

static inline int
od_storage_watchdog_do_tsa_polling(od_storage_watchdog_t *watchdog)
{
	od_rule_storage_t *storage;
	storage = watchdog->storage;

	od_global_t *global;
	global = watchdog->global;

	od_instance_t *instance;
	instance = global->instance;

	/* no need to update last tsa if it will not be checked */
	if (storage == NULL ||
	    storage->target_session_attrs == OD_TARGET_SESSION_ATTRS_ANY) {
		return OK_RESPONSE;
	}

	if (storage->endpoints_count == 0) {
		return OK_RESPONSE;
	}

	tsa_endpoint_update_arg_t *args = malloc(
		sizeof(tsa_endpoint_update_arg_t) * storage->endpoints_count);
	if (args == NULL) {
		return NOT_OK_RESPONSE;
	}
	memset(args, 0,
	       sizeof(tsa_endpoint_update_arg_t) * storage->endpoints_count);

	machine_wait_group_t *wg = machine_wait_group_create();
	if (wg == NULL) {
		free(args);
		return NOT_OK_RESPONSE;
	}

	for (size_t i = 0; i < storage->endpoints_count; ++i) {
		tsa_endpoint_update_arg_t *arg = &args[i];
		arg->wg = wg;
		arg->endpoint = &storage->endpoints[i];
		arg->endpoint_idx = i;
		arg->watchdog = watchdog;
		arg->status = OK_RESPONSE;

		machine_wait_group_add(wg);

		int64_t coroutine_id;
		coroutine_id = machine_coroutine_create_named(
			od_storage_watchdog_update_tsa_on_endpoint, arg,
			"tsa-updater");
		if (coroutine_id == -1) {
			od_error(&instance->logger, "watchdog", NULL, NULL,
				 "can't create tsa updater coroutine");
			free(args);
			return NOT_OK_RESPONSE;
		}
	}

	if (machine_wait_group_wait(wg, UINT32_MAX) != 0) {
		od_error(&instance->logger, "watchdog", NULL, NULL,
			 "tsa updaters wait timeout or cancel");
		free(args);
		return NOT_OK_RESPONSE;
	}

	for (size_t i = 0; i < storage->endpoints_count; ++i) {
		tsa_endpoint_update_arg_t *arg = &args[i];
		od_retcode_t st = arg->status;
		if (st != OK_RESPONSE) {
			od_error(&instance->logger, "watchdog", NULL, NULL,
				 "one of tsa updaters failed");
			free(args);
			return st;
		}
	}

	free(args);
	return OK_RESPONSE;
}

static inline void
od_storage_watchdog_do_polling_step(od_storage_watchdog_t *watchdog)
{
	od_global_t *global;
	global = watchdog->global;

	od_instance_t *instance;
	instance = global->instance;

	if (watchdog->query) {
		if (od_storage_watchdog_do_lag_polling(watchdog) !=
		    OK_RESPONSE) {
			od_error(&instance->logger, "watchdog", NULL, NULL,
				 "lag polling failed");
		}
	}

	if (od_storage_watchdog_do_tsa_polling(watchdog) != OK_RESPONSE) {
		od_error(&instance->logger, "watchdog", NULL, NULL,
			 "tsa polling failed");
	}
}

static inline void
od_storage_watchdog_do_polling_loop(od_storage_watchdog_t *watchdog)
{
	while (od_storage_watchdog_is_online(watchdog)) {
		od_storage_watchdog_do_polling_step(watchdog);

		machine_sleep(1000);
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
	od_atomic_u64_set(&watchdog->finished, 1ULL);
}
