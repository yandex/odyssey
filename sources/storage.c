
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

od_storage_watchdog_t *od_storage_watchdog_allocate(od_global_t *global)
{
	od_storage_watchdog_t *watchdog;
	watchdog = malloc(sizeof(od_storage_watchdog_t));
	if (watchdog == NULL) {
		return NULL;
	}
	memset(watchdog, 0, sizeof(od_storage_watchdog_t));
	watchdog->check_retry = 10;
	watchdog->global = global;

	return watchdog;
}

int od_storage_watchdog_free(od_storage_watchdog_t *watchdog)
{
	if (watchdog == NULL) {
		return NOT_OK_RESPONSE;
	}

	if (watchdog->query) {
		free(watchdog->query);
	}

	free(watchdog);
	return OK_RESPONSE;
}

od_rule_storage_t *od_rules_storage_allocate(void)
{
	od_rule_storage_t *storage;
	storage = (od_rule_storage_t *)malloc(sizeof(*storage));
	if (storage == NULL)
		return NULL;
	memset(storage, 0, sizeof(*storage));
	storage->tls_opts = od_tls_opts_alloc();
	if (storage->tls_opts == NULL) {
		return NULL;
	}

	od_list_init(&storage->link);
	return storage;
}

void od_rules_storage_free(od_rule_storage_t *storage)
{
	if (storage->name)
		free(storage->name);
	if (storage->type)
		free(storage->type);
	if (storage->host)
		free(storage->host);

	if (storage->tls_opts) {
		od_tls_opts_free(storage->tls_opts);
	}

	if (storage->watchdog) {
		od_storage_watchdog_free(storage->watchdog);
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
	return copy;
error:
	od_rules_storage_free(copy);
	return NULL;
}

static inline int
od_storage_watchdog_parse_lag_from_datarow(od_logger_t *logger,
					   machine_msg_t *msg, int *repl_lag)
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

	if (count != 2)
		goto error;

	/* colname (not used) */
	uint32_t hb;
	rc = kiwi_read32(&hb, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1)) {
		goto error;
	}

	*repl_lag = hb;

	return OK_RESPONSE;
error:
	return NOT_OK_RESPONSE;
}

static inline int od_router_update_heartbit_cb(od_route_t *route, void **argv)
{
	od_route_lock(route);
	route->last_heartbit = argv[0];
	od_route_unlock(route);
	return 0;
}

void od_storage_watchdog_watch(od_storage_watchdog_t *watchdog)
{
	od_global_t *global = watchdog->global;
	od_router_t *router = global->router;
	od_instance_t *instance = global->instance;

	/* create internal auth client */
	od_client_t *watchdog_client;
	watchdog_client = od_client_allocate();
	if (watchdog_client == NULL)
		return -1;
	watchdog_client->global = global;
	watchdog_client->type = OD_POOL_CLIENT_INTERNAL;
	od_id_generate(&watchdog_client->id, "a");

	/* set storage user and database */
	kiwi_var_set(&watchdog_client->startup.user, KIWI_VAR_UNDEF,
		     watchdog->storage_user,
		     strlen(watchdog->storage_user) + 1);

	kiwi_var_set(&watchdog_client->startup.database, KIWI_VAR_UNDEF,
		     watchdog->storage_db, strlen(watchdog->storage_db) + 1);

	machine_msg_t *msg;

	int last_heartbit = 0;
	int rc;
	/* route */
	od_router_status_t status;
	status = od_router_route(router, watchdog_client);
	if (status != OD_ROUTER_OK) {
		od_client_free(watchdog_client);
		return -1;
	}
	od_rule_t *rule = watchdog_client->rule;

	/* attach */
	status = od_router_attach(router, watchdog_client, false);
	if (status != OD_ROUTER_OK) {
		od_router_unroute(router, watchdog_client);
		od_client_free(watchdog_client);
		return NOT_OK_RESPONSE;
	}
	od_server_t *server;
	server = watchdog_client->server;

	for (;;) {
		od_debug(&instance->logger, "auth_query", NULL, server,
			 "attached to server %s%.*s", server->id.id_prefix,
			 (int)sizeof(server->id.id), server->id.id);

		for (int retry = 0; retry < watchdog->check_retry; ++retry) {
			msg = od_query_do(server, watchdog->query, NULL);
			rc = od_storage_watchdog_parse_lag_from_datarow(
				&instance->logger, msg, &last_heartbit);
			machine_msg_free(msg);
			if (rc == OK_RESPONSE) {
				// retry
				void *argv[] = { last_heartbit };
				od_router_foreach(router,
						  od_router_update_heartbit_cb,
						  argv);
				break;
			}
		}

		/* 1 second soft interval */
		machine_sleep(watchdog->interval);
	}
}
