
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>
#include <machinarium/wait_list.h>
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

static od_rule_t *od_auth_query_source(od_router_t *router, od_rule_t *rule)
{
	kiwi_be_startup_t startup;
	kiwi_be_startup_init(&startup);
	if (kiwi_var_set(&startup.user, KIWI_VAR_UNDEF, rule->auth_query_user,
			 strlen(rule->auth_query_user) + 1) == -1 ||
	    kiwi_var_set(&startup.database, KIWI_VAR_UNDEF, rule->auth_query_db,
			 strlen(rule->auth_query_db) + 1) == -1) {
		return NULL;
	}

	od_router_lock(router);
	od_rule_t *source = od_rules_forward(&router->rules, &startup, NULL, 1);
	if (source != NULL) {
		od_rules_ref(source);
	}
	od_router_unlock(router);
	return source;
}

static od_client_t *create_auth_client(od_global_t *global,
				       od_instance_t *instance, od_rule_t *rule,
				       od_rule_t *source)
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

	if (client->rule != source) {
		od_router_unroute(router, client);
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
	od_assert(server != NULL);

	machine_msg_t *msg;
	if (od_query_do(server, "auth_query", query, user, 500, &msg) !=
	    OK_RESPONSE) {
		od_error(&instance->logger, "auth_query", client, server,
			 "auth query failed");
		return NOT_OK_RESPONSE;
	}
	if (msg == NULL) {
		password->password = NULL;
		password->password_len = 0;
		return OK_RESPONSE;
	}

	int rc = od_auth_parse_passwd_from_datarow(&instance->logger, msg,
						   password);
	machine_msg_free(msg);

	return rc;
}

static int get_password_from_auth_query(od_global_t *global,
					od_instance_t *instance,
					od_rule_t *rule, od_rule_t *source,
					char *user, const char *auth_query,
					kiwi_password_t *password)
{
	od_router_t *router = global->router;

	od_client_t *client =
		create_auth_client(global, instance, rule, source);
	if (client == NULL) {
		return NOT_OK_RESPONSE;
	}

	int rc = do_auth_query(instance, client, user, auth_query, password);

	od_router_close(router, client);
	od_router_unroute(router, client);
	od_client_free_extended(client);

	return rc;
}

#define OD_AUTH_QUERY_CACHE_SIZE 64

typedef struct {
	od_list_t link;
	char user[KIWI_MAX_VAR_SIZE];
	char query[OD_QRY_MAX_SZ];
	od_rule_t *source;
	od_route_pswd_t *password;
	uint64_t refresh_at_ms;
	int refresh_in_progress;
	int users;
	atomic_uint_fast64_t version;
	mm_wait_list_t waiters;
} od_auth_query_cache_entry_t;

static void od_auth_query_cache_entry_free(od_auth_query_cache_entry_t *entry)
{
	od_rules_unref(entry->source);
	od_route_pswd_unref(entry->password);
	mm_wait_list_destroy(&entry->waiters);
	od_free(entry);
}

void od_auth_query_cache_free(od_route_t *route)
{
	od_list_t *i, *n;
	od_list_foreach_safe (&route->auth_query_cache.entries, i, n) {
		od_auth_query_cache_entry_t *entry;
		entry = od_container_of(i, od_auth_query_cache_entry_t, link);
		od_list_unlink(&entry->link);
		od_auth_query_cache_entry_free(entry);
	}
	mm_wait_list_destroy(&route->auth_query_cache.available);
}

static void od_auth_query_cache_notify(od_route_t *route)
{
	atomic_fetch_add(&route->auth_query_cache.version, 1);
	mm_wait_list_notify_all(&route->auth_query_cache.available);
}

/* Called and returns with the route locked. */
static int od_auth_query_wait(od_route_t *route, mm_wait_list_t *waiters,
			      uint64_t version, uint64_t start_ms)
{
	uint64_t elapsed_ms = machine_time_ms() - start_ms;
	if (elapsed_ms >= 500) {
		return -1;
	}
	od_route_unlock(route);
	int rc = mm_wait_list_compare_wait(waiters, NULL, version,
					   500 - elapsed_ms);
	int err = machine_errno();
	od_route_lock(route);
	return rc == 0 || err == EAGAIN ? 0 : -1;
}

static od_auth_query_cache_entry_t *
od_auth_query_cache_get(od_route_t *route, od_rule_t *source, const char *user,
			const char *query, uint64_t start_ms)
{
	od_auth_query_cache_entry_t *oldest;
	od_list_t *i;
retry:
	oldest = NULL;
	od_list_foreach (&route->auth_query_cache.entries, i) {
		od_auth_query_cache_entry_t *entry;
		entry = od_container_of(i, od_auth_query_cache_entry_t, link);
		if (entry->source == source && strcmp(entry->user, user) == 0 &&
		    strcmp(entry->query, query) == 0) {
			od_list_unlink(&entry->link);
			od_list_append(&route->auth_query_cache.entries,
				       &entry->link);
			return entry;
		}
		if (oldest == NULL && !entry->refresh_in_progress &&
		    entry->users == 0) {
			oldest = entry;
		}
	}

	if (route->auth_query_cache.count == OD_AUTH_QUERY_CACHE_SIZE &&
	    oldest == NULL) {
		uint64_t version =
			atomic_load(&route->auth_query_cache.version);
		if (od_auth_query_wait(route,
				       &route->auth_query_cache.available,
				       version, start_ms) == -1) {
			return NULL;
		}
		goto retry;
	}

	od_auth_query_cache_entry_t *entry = od_malloc(sizeof(*entry));
	if (entry == NULL) {
		return NULL;
	}
	memset(entry, 0, sizeof(*entry));
	strcpy(entry->user, user);
	strcpy(entry->query, query);
	entry->source = source;
	od_rules_ref(source);
	atomic_init(&entry->version, 0);
	mm_wait_list_init(&entry->waiters, &entry->version);

	if (route->auth_query_cache.count == OD_AUTH_QUERY_CACHE_SIZE) {
		od_list_unlink(&oldest->link);
		od_auth_query_cache_entry_free(oldest);
		route->auth_query_cache.count--;
	}
	od_list_append(&route->auth_query_cache.entries, &entry->link);
	route->auth_query_cache.count++;
	return entry;
}

typedef struct {
	od_global_t *global;
	od_instance_t *instance;
	od_rule_t *rule;
	od_route_t *route;
	od_auth_query_cache_entry_t *entry;
} refresh_arg_t;

static void pswd_update_task(void *a)
{
	refresh_arg_t *arg = a;

	od_global_t *global = arg->global;
	od_instance_t *instance = arg->instance;
	od_rule_t *rule = arg->rule;
	od_route_t *route = arg->route;
	od_auth_query_cache_entry_t *entry = arg->entry;

	kiwi_password_t password;
	memset(&password, 0, sizeof(password));
	int rc = get_password_from_auth_query(global, instance, rule,
					      entry->source, entry->user,
					      entry->query, &password);

	uint64_t jitter = ((uint64_t)machine_lrand48()) % 5000;
	uint64_t until = machine_time_ms() + 10 * 1000 + jitter;

	od_route_lock(route);

	entry->refresh_at_ms = machine_time_ms() + 1000;

	if (rc == OK_RESPONSE) {
		od_route_pswd_t *new = od_route_pswd_create(password.password);
		if (new != NULL) {
			od_route_pswd_unref(entry->password);
			entry->password = new;
			entry->refresh_at_ms = until;
		}
	}
	od_free(password.password);

	entry->refresh_in_progress = 0;
	route->auth_query_cache.refresh_in_progress--;
	atomic_fetch_add(&entry->version, 1);
	mm_wait_list_notify_all(&entry->waiters);
	if (entry->users == 0) {
		od_auth_query_cache_notify(route);
	}

	od_route_unlock(route);

	od_rules_unref(rule);
	od_free(arg);
}

static int run_refresh_task(od_global_t *global, od_instance_t *instance,
			    od_rule_t *rule, od_route_t *route,
			    od_client_t *client,
			    od_auth_query_cache_entry_t *entry)
{
	entry->refresh_in_progress = 1;
	route->auth_query_cache.refresh_in_progress++;

	od_rules_ref(rule);

	refresh_arg_t *arg = od_malloc(sizeof(refresh_arg_t));
	if (arg == NULL) {
		goto error;
	}
	memset(arg, 0, sizeof(refresh_arg_t));

	arg->global = global;
	arg->instance = instance;
	arg->route = route;
	arg->rule = rule;
	arg->entry = entry;

	int64_t coroid = machine_coroutine_create_named(pswd_update_task, arg,
							"auth-query");
	if (coroid == -1) {
		goto error;
	}

	return 0;

error:
	od_rules_unref(rule);
	od_free(arg);
	entry->refresh_in_progress = 0;
	route->auth_query_cache.refresh_in_progress--;
	int err = mm_errno_get();
	if (entry->users == 0) {
		od_auth_query_cache_notify(route);
	}
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

	od_assert(route != NULL);
	od_assert(rule->auth_query != NULL);

	char query[OD_QRY_MAX_SZ];
	if (od_query_format(rule->auth_query,
			    rule->auth_query + strlen(rule->auth_query),
			    &client->startup.user, peer, query,
			    sizeof(query)) == -1) {
		od_error(&instance->logger, "auth_query", client, NULL,
			 "auth query is too long");
		return -1;
	}

	uint64_t start_ms = machine_time_ms();
	od_rule_t *source = od_auth_query_source(global->router, rule);
	if (source == NULL) {
		od_error(&instance->logger, "auth_query", client, NULL,
			 "can't match auth query source");
		return -1;
	}
	od_route_lock(route);

	od_auth_query_cache_entry_t *entry = od_auth_query_cache_get(
		route, source, client->startup.user.value, query, start_ms);
	od_rules_unref(source);
	if (entry == NULL) {
		od_error(&instance->logger, "auth_query", client, NULL,
			 "can't acquire auth query cache entry");
		od_route_unlock(route);
		return -1;
	}

	uint64_t now_ms = machine_time_ms();
	if (!entry->refresh_in_progress &&
	    (entry->password == NULL || now_ms >= entry->refresh_at_ms)) {
		(void)run_refresh_task(global, instance, rule, route, client,
				       entry);
	}

	if (entry->password == NULL) {
		if (entry->refresh_in_progress) {
			uint64_t version = atomic_load(&entry->version);
			entry->users++;
			while (entry->refresh_in_progress &&
			       atomic_load(&entry->version) == version) {
				if (od_auth_query_wait(route, &entry->waiters,
						       version,
						       start_ms) == -1) {
					break;
				}
			}
			entry->users--;
			if (entry->users == 0 && !entry->refresh_in_progress) {
				od_auth_query_cache_notify(route);
			}
		}

		if (entry->password == NULL) {
			od_error(
				&instance->logger, "auth_query", client, NULL,
				"timeout on wait for password to become available or other error");
			od_route_unlock(route);
			return -1;
		}
	}

	od_route_pswd_t *cached = od_route_pswd_ref(entry->password);
	od_route_unlock(route);

	if (cached->is_null) {
		od_route_pswd_unref(cached);
		return 0;
	}

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
