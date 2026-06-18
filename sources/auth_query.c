
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
				       od_instance_t *instance, od_rule_t *rule)
{
	od_router_t *router = global->router;

	od_client_t *client = od_client_allocate_internal(global, "auth-query");
	if (client == NULL) {
		od_error(&instance->logger, "auth_query", NULL, NULL,
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

static int do_auth_query(od_instance_t *instance, od_client_t *client,
			 char *user, const char *query,
			 kiwi_password_t *password)
{
	od_server_t *server = client->server;
	assert(server != NULL);

	machine_msg_t *msg =
		od_query_do(server, "auth_query", query, user, 500);
	if (msg == NULL) {
		od_error(&instance->logger, "auth_query", client, server,
			 "auth query returned empty msg");
		return NOT_OK_RESPONSE;
	}

	int rc = od_auth_parse_passwd_from_datarow(&instance->logger, msg,
						   password);
	machine_msg_free(msg);

	return rc;
}

static int get_password_from_auth_query(od_global_t *global,
					od_instance_t *instance,
					od_rule_t *rule, char *user,
					const char *auth_query,
					kiwi_password_t *password)
{
	od_router_t *router = global->router;

	od_client_t *client = create_auth_client(global, instance, rule);
	if (client == NULL) {
		return NOT_OK_RESPONSE;
	}

	int rc = do_auth_query(instance, client, user, auth_query, password);

	od_router_close(router, client);
	od_router_unroute(router, client);
	od_client_free_extended(client);

	return rc;
}

typedef struct {
	od_global_t *global;
	od_instance_t *instance;
	od_rule_t *rule;
	od_route_t *route;
	char user[KIWI_MAX_VAR_SIZE];
	char query[OD_QRY_MAX_SZ];
} refresh_arg_t;

static void pswd_update_task(void *a)
{
	refresh_arg_t *arg = a;

	od_global_t *global = arg->global;
	od_instance_t *instance = arg->instance;
	od_rule_t *rule = arg->rule;
	od_route_t *route = arg->route;

	kiwi_password_t password;
	memset(&password, 0, sizeof(password));
	int rc = get_password_from_auth_query(global, instance, rule, arg->user,
					      arg->query, &password);

	uint64_t jitter = ((uint64_t)machine_lrand48()) % 5000;
	uint64_t until = machine_time_ms() + 10 * 1000 + jitter;

	od_route_lock(route);

	route->auth_query_cache.valid_until_ms = machine_time_ms() + 1000;

	if (rc == OK_RESPONSE) {
		od_route_pswd_t *new = od_route_pswd_create(password.password);
		od_free(password.password);
		if (new != NULL) {
			od_route_pswd_unref(route->auth_query_cache.password);
			route->auth_query_cache.password = new;
			route->auth_query_cache.valid_until_ms = until;
		}
	}

	route->auth_query_cache.refresh_in_progress = 0;

	mm_wait_flag_set(route->auth_query_cache.ready);

	od_route_unlock(route);

	od_rules_unref(rule);
	od_free(arg);
}

static int run_refresh_task(od_global_t *global, od_instance_t *instance,
			    od_rule_t *rule, od_route_t *route,
			    od_client_t *client, char *peer)
{
	route->auth_query_cache.refresh_in_progress = 1;

	od_rules_ref(rule);

	refresh_arg_t *arg = od_malloc(sizeof(refresh_arg_t));
	if (arg == NULL) {
		goto error;
	}
	memset(arg, 0, sizeof(refresh_arg_t));

	kiwi_var_t *user = &client->startup.user;

	arg->global = global;
	arg->instance = instance;
	arg->route = route;
	arg->rule = rule;
	strcpy(arg->user, user->value);

	char *format_pos = rule->auth_query;
	char *format_end = rule->auth_query + strlen(rule->auth_query);
	od_query_format(format_pos, format_end, user, peer, arg->query,
			sizeof(arg->query));

	int64_t coroid = machine_coroutine_create_named(pswd_update_task, arg,
							"auth-query");
	if (coroid == -1) {
		goto error;
	}

	return 0;

error:
	od_rules_unref(rule);
	od_free(arg);
	route->auth_query_cache.refresh_in_progress = 0;
	int err = mm_errno_get();
	od_error(&instance->logger, "auth_query", client, NULL,
		 "can't run auth query coroutine: %d (%s)", err, strerror(err));

	return -1;
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

	od_route_pswd_t *cached =
		od_route_pswd_ref(route->auth_query_cache.password);
	if (cached != NULL) {
		/*
		 * password was queried at least once
		 * reuse it and maybe run refresh coro in background
		 */
		uint64_t now_ms = machine_time_ms();
		int outdated = route->auth_query_cache.valid_until_ms < now_ms;
		if (outdated && !route->auth_query_cache.refresh_in_progress) {
			(void)run_refresh_task(global, instance, rule, route,
					       client, peer);
		}
	} else {
		/*
		 * this is the first time password is acquired
		 *
		 * if no one run the refresh - do it, and the wait for ready
		 */

		int rc = 0;
		if (!route->auth_query_cache.refresh_in_progress) {
			rc = run_refresh_task(global, instance, rule, route,
					      client, peer);
		}

		if (rc == 0) {
			od_route_unlock(route);
			mm_wait_flag_wait(route->auth_query_cache.ready, 500);
			od_route_lock(route);
		}

		cached = od_route_pswd_ref(route->auth_query_cache.password);
		if (cached == NULL) {
			od_error(
				&instance->logger, "auth_query", client, NULL,
				"timeout on wait for password to become available or other error");
			od_route_unlock(route);
			return -1;
		}
	}

	od_route_unlock(route);

	client->password.password = od_strdup(cached->value);
	od_route_pswd_unref(cached);
	if (client->password.password == NULL) {
		od_error(&instance->logger, "auth_query", client, NULL,
			 "can't reuse password: OOM");
		return -1;
	}
	client->password.password_len = strlen(client->password.password) + 1;

	return 0;
}
