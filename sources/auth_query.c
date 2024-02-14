
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

static inline int od_auth_parse_passwd_from_datarow(od_logger_t *logger,
						    machine_msg_t *msg,
						    kiwi_password_t *result)
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

	// The length of the column value, in bytes (this count does not include itself).
	// Can be zero.
	// As a special case, -1 indicates a NULL column value. No value bytes follow in the NULL case.
	uint32_t password_len;
	rc = kiwi_read32(&password_len, &pos, &pos_size);

	if (kiwi_unlikely(rc == -1)) {
		goto error;
	}
	// --1
	if (password_len == UINT_MAX) {
		result->password = NULL;
		result->password_len = password_len + 1;

		od_debug(logger, "query", NULL, NULL,
			 "auth query returned empty password for user %.*s",
			 user_len, user);
		goto success;
	}

	if (password_len > ODYSSEY_AUTH_QUERY_MAX_PASSSWORD_LEN) {
		goto error;
	}

	char *password = pos;
	rc = kiwi_readn(password_len, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1)) {
		goto error;
	}

	result->password = malloc(password_len + 1);
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

int od_auth_query(od_client_t *client, char *peer)
{
	od_global_t *global = client->global;
	od_rule_t *rule = client->rule;
	od_rule_storage_t *storage = rule->storage;
	kiwi_var_t *user = &client->startup.user;
	kiwi_password_t *password = &client->password;
	od_instance_t *instance = global->instance;
	od_router_t *router = global->router;

	/* check odyssey storage auh query cache before
	* doing any actual work
	*/
	/* username -> password cache */
	od_hashmap_elt_t *value;
	od_hashmap_elt_t key;
	od_auth_cache_value_t *cache_value;
	od_hash_t keyhash;
	uint64_t current_time;

	key.data = user->value;
	key.len = user->value_len;

	keyhash = od_murmur_hash(key.data, key.len);
	/* acquire hash map entry lock */
	value = od_hashmap_lock_key(storage->acache, keyhash, &key);

	if (value->data == NULL) {
		/* one-time initialize */
		value->len = sizeof(od_auth_cache_value_t);
		value->data = malloc(value->len);
		/* OOM */
		if (value->data == NULL) {
			goto error;
		}
		memset(((od_auth_cache_value_t *)(value->data)), 0, value->len);
	}

	cache_value = (od_auth_cache_value_t *)value->data;

	current_time = machine_time_us();

	if (/* password cached for 10 sec */
	    current_time - cache_value->timestamp < 10 * interval_usec) {
		od_debug(&instance->logger, "auth_query", NULL, NULL,
			 "reusing cached password for user %.*s",
			 user->value_len, user->value);
		/* unlock hashmap entry */
		password->password_len = cache_value->passwd_len;
		if (cache_value->passwd_len > 0) {
			/*  */
			password->password = malloc(password->password_len + 1);
			if (password->password == NULL) {
				goto error;
			}
			strncpy(password->password, cache_value->passwd,
				cache_value->passwd_len);
			password->password[password->password_len] = '\0';
		}
		od_hashmap_unlock_key(storage->acache, keyhash, &key);
		return OK_RESPONSE;
	}

	/* create internal auth client */
	od_client_t *auth_client;

	auth_client = od_client_allocate_internal(global, "auth-query");

	if (auth_client == NULL) {
		od_debug(&instance->logger, "auth_query", auth_client, NULL,
			 "failed to allocate internal auth query client");
		goto error;
	}

	od_debug(&instance->logger, "auth_query", auth_client, NULL,
		 "acquiring password for user %.*s", user->value_len,
		 user->value);

	/* set auth query route user and database */
	kiwi_var_set(&auth_client->startup.user, KIWI_VAR_UNDEF,
		     rule->auth_query_user, strlen(rule->auth_query_user) + 1);

	kiwi_var_set(&auth_client->startup.database, KIWI_VAR_UNDEF,
		     rule->auth_query_db, strlen(rule->auth_query_db) + 1);

	/* set io from client */
	od_io_t auth_client_io = auth_client->io;
	auth_client->io = client->io;

	/* route */
	od_router_status_t status;
	status = od_router_route(router, auth_client);

	/* return io auth_client back */
	auth_client->io = auth_client_io;

	if (status != OD_ROUTER_OK) {
		od_debug(&instance->logger, "auth_query", auth_client, NULL,
			 "failed to route internal auth query client: %s",
			 od_router_status_to_str(status));
		od_client_free(auth_client);
		goto error;
	}

	/* attach */
	status = od_router_attach(router, auth_client, false);
	if (status != OD_ROUTER_OK) {
		od_debug(
			&instance->logger, "auth_query", auth_client, NULL,
			"failed to attach internal auth query client to route: %s",
			od_router_status_to_str(status));
		od_router_unroute(router, auth_client);
		od_client_free(auth_client);
		goto error;
	}
	od_server_t *server;
	server = auth_client->server;

	od_debug(&instance->logger, "auth_query", auth_client, server,
		 "attached to server %s%.*s", server->id.id_prefix,
		 (int)sizeof(server->id.id), server->id.id);

	/* connect to server, if necessary */
	int rc;
	if (server->io.io == NULL) {
		/* acquire new backend connection for auth query */
		rc = od_backend_connect(server, "auth_query", NULL,
					auth_client);
		if (rc == NOT_OK_RESPONSE) {
			od_debug(&instance->logger, "auth_query", auth_client,
				 server,
				 "failed to acquire backend connection: %s",
				 od_io_error(&server->io));
			od_router_close(router, auth_client);
			od_router_unroute(router, auth_client);
			od_client_free(auth_client);
			goto error;
		}
	}

	/* preformat and execute query */
	char query[OD_QRY_MAX_SZ];
	char *format_pos = rule->auth_query;
	char *format_end = rule->auth_query + strlen(rule->auth_query);
	od_query_format(format_pos, format_end, user, peer, query,
			sizeof(query));

	machine_msg_t *msg;
	msg = od_query_do(server, "auth_query", query, user->value);
	if (msg == NULL) {
		od_log(&instance->logger, "auth_query", auth_client, server,
		       "auth query returned empty msg");
		od_router_close(router, auth_client);
		od_router_unroute(router, auth_client);
		od_client_free(auth_client);
		goto error;
	}
	if (od_auth_parse_passwd_from_datarow(&instance->logger, msg,
					      password) == NOT_OK_RESPONSE) {
		od_debug(&instance->logger, "auth_query", auth_client, server,
			 "auth query returned datarow in incompatable format");
		od_router_close(router, auth_client);
		od_router_unroute(router, auth_client);
		od_client_free(auth_client);
		goto error;
	}

	/* save received password and recieve timestamp */
	if (cache_value->passwd != NULL) {
		/* drop previous value */
		free(cache_value->passwd);

		// there should be cache_value->passwd = NULL for sanity
		// but this is meaninigless sinse we assing new value just below
	}
	cache_value->passwd_len = password->password_len;
	cache_value->passwd = malloc(password->password_len);
	if (cache_value->passwd == NULL) {
		goto error;
	}
	strncpy(cache_value->passwd, password->password,
		cache_value->passwd_len);

	cache_value->timestamp = current_time;

	/* detach and unroute */
	od_router_detach(router, auth_client);
	od_router_unroute(router, auth_client);
	od_client_free(auth_client);
	od_hashmap_unlock_key(storage->acache, keyhash, &key);
	return OK_RESPONSE;

error:
	/* unlock hashmap entry */
	od_hashmap_unlock_key(storage->acache, keyhash, &key);
	return NOT_OK_RESPONSE;
}
