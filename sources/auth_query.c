
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>
#include <kiwi/kiwi.h>

#include <status.h>
#include <types.h>
#include <auth_query.h>
#include <logger.h>
#include <attach.h>
#include <client.h>
#include <internal_client.h>
#include <global.h>
#include <hashmap.h>
#include <pool.h>
#include <storage.h>
#include <router.h>
#include <instance.h>
#include <query.h>

static int od_auth_parse_passwd_from_datarow(od_logger_t *logger,
					     machine_msg_t *msg,
					     kiwi_password_t *result)
{
	char *pos = (char *)machine_msg_data(msg) + 1;
	uint32_t pos_size = machine_msg_size(msg) - 1;

	/* size */
	uint32_t size;
	int rc;
	rc = kiwi_read32(&size, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1)) {
		goto error;
	}
	/* count */
	uint16_t count;
	rc = kiwi_read16(&count, &pos, &pos_size);

	if (kiwi_unlikely(rc == -1)) {
		goto error;
	}

	if (count != 2) {
		goto error;
	}

	/* user (not used) */
	uint32_t user_len;
	rc = kiwi_read32(&user_len, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1)) {
		goto error;
	}
	char *user = pos;
	rc = kiwi_readn(user_len, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1)) {
		goto error;
	}

	(void)user;
	(void)user_len;

	/* password */

	/*
	 * The length of the column value, in bytes (this count does not include itself).
	 * Can be zero.
	 * As a special case, -1 indicates a NULL column value. No value bytes follow in the NULL case.
	 */
	uint32_t password_len;
	rc = kiwi_read32(&password_len, &pos, &pos_size);

	if (kiwi_unlikely(rc == -1)) {
		goto error;
	}

	/* the case of -1 */
	if (password_len == UINT_MAX) {
		result->password = NULL;
		result->password_len = password_len + 1;

		od_debug(logger, "query", NULL, NULL,
			 "auth query returned empty password for user %.*s",
			 user_len, user);
		goto success;
	}

	if (password_len > ODYSSEY_AUTH_QUERY_MAX_PASSWORD_LEN) {
		goto error;
	}

	char *password = pos;
	rc = kiwi_readn(password_len, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1)) {
		goto error;
	}

	result->password = od_malloc(password_len + 1);
	if (result->password == NULL) {
		goto error;
	}
	memcpy(result->password, password, password_len);
	result->password[password_len] = 0;
	result->password_len = password_len + 1;

success:
	return OK_RESPONSE;
error:
	return NOT_OK_RESPONSE;
}

static od_client_t *create_auth_client(od_global_t *global,
				       od_instance_t *instance, od_rule_t *rule,
				       od_client_t *orig_client)
{
	od_router_t *router = global->router;

	od_client_t *client = od_client_allocate_internal(global, "auth-query");
	if (client == NULL) {
		od_error(&instance->logger, "auth_query", orig_client, NULL,
			 "can't allocate auth client");
		return NULL;
	}

	kiwi_var_set(&client->startup.user, KIWI_VAR_UNDEF,
		     rule->auth_query_user, strlen(rule->auth_query_user) + 1);
	kiwi_var_set(&client->startup.database, KIWI_VAR_UNDEF,
		     rule->auth_query_db, strlen(rule->auth_query_db) + 1);

	od_router_status_t status;
	status = od_router_route(router, client);
	if (status != OD_ROUTER_OK) {
		od_debug(&instance->logger, "auth_query", client, NULL,
			 "failed to route internal auth query client: %s",
			 od_router_status_to_str(status));
		od_client_free_extended(client);
		return NULL;
	}

	int rc = od_attach_extended(instance, "auth_query", router, client);
	if (rc != OK_RESPONSE) {
		od_router_unroute(router, client);
		od_client_free_extended(client);
		return NULL;
	}

	return client;
}

static int do_auth_query(od_instance_t *instance, od_rule_t *rule,
			 od_route_t *route, od_client_t *client,
			 od_client_t *orig_client, char *peer)
{
	od_server_t *server = client->server;
	assert(server != NULL);

	kiwi_var_t *user = &orig_client->startup.user;

	char query[OD_QRY_MAX_SZ];
	char *format_pos = rule->auth_query;
	char *format_end = rule->auth_query + strlen(rule->auth_query);
	od_query_format(format_pos, format_end, user, peer, query,
			sizeof(query));

	machine_msg_t *msg =
		od_query_do(server, "auth_query", query, user->value, 500);
	if (msg == NULL) {
		od_error(&instance->logger, "auth_query", client, server,
			 "auth query returned empty msg");
		return NOT_OK_RESPONSE;
	}

	kiwi_password_t password;
	int rc = od_auth_parse_passwd_from_datarow(&instance->logger, msg,
						   &password);
	machine_msg_free(msg);

	if (rc != OK_RESPONSE) {
		return rc;
	}

	od_free(route->auth_query_cache.password);
	route->auth_query_cache.password = password.password;
	route->auth_query_cache.valid_until_ms = machine_time_ms() + 10 * 1000;

	return OK_RESPONSE;
}

static int do_update_auth_cache(od_global_t *global, od_instance_t *instance,
				od_rule_t *rule, od_route_t *route,
				od_client_t *orig_client, char *peer)
{
	od_router_t *router = global->router;

	od_client_t *client =
		create_auth_client(global, instance, rule, orig_client);
	if (client == NULL) {
		return NOT_OK_RESPONSE;
	}

	od_debug(&instance->logger, "auth_query", orig_client, NULL,
		 "got auth client %s%.*s", client->id.id_prefix,
		 (int)sizeof(client->id.id), client->id.id);

	int rc =
		do_auth_query(instance, rule, route, client, orig_client, peer);

	od_router_close(router, client);
	od_router_unroute(router, client);
	od_client_free_extended(client);

	return rc;
}

static int get_password_route_locked(od_global_t *global,
				     od_instance_t *instance, od_rule_t *rule,
				     od_route_t *route, od_client_t *client,
				     char *peer)
{
	uint64_t now_ms = machine_time_ms();

	if (route->auth_query_cache.valid_until_ms < now_ms) {
		od_debug(&instance->logger, "auth_query", client, NULL,
			 "need to update cached password");

		int rc = do_update_auth_cache(global, instance, rule, route,
					      client, peer);
		if (rc != OK_RESPONSE) {
			return rc;
		}
	}

	client->password.password = od_strdup(route->auth_query_cache.password);
	if (client->password.password == NULL) {
		od_error(&instance->logger, "auth_query", client, NULL,
			 "can't reuse password: OOM");
		return -1;
	}
	client->password.password_len = strlen(client->password.password) + 1;

	return 0;
}

int od_auth_query(od_client_t *client, char *peer)
{
	od_global_t *global = client->global;
	od_instance_t *instance = global->instance;
	od_route_t *route = client->route;
	od_rule_t *rule = client->rule;

	assert(route != NULL);
	assert(rule->auth_query != NULL);

	od_route_lock(route);
	int rc = get_password_route_locked(global, instance, rule, route,
					   client, peer);
	od_route_unlock(route);

	return rc;
}
