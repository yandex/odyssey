
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

#include <odyssey.h>

static inline int
od_auth_frontend_cleartext(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;

	/* AuthenticationCleartextPassword */
	machine_msg_t *msg;
	msg = kiwi_be_write_authentication_clear_text(NULL);
	if (msg == NULL)
		return -1;
	int rc;
	rc = od_write(&client->io, msg);
	if (rc == -1) {
		od_error(&instance->logger, "auth", client, NULL,
		         "write error: %s",
		         od_io_error(&client->io));
		return -1;
	}

	/* wait for password response */
	while (1) {
		msg = od_read(&client->io, UINT32_MAX);
		if (msg == NULL) {
			od_error(&instance->logger, "auth", client, NULL,
			         "read error: %s",
			         od_io_error(&client->io));
			return -1;
		}
		kiwi_fe_type_t type = *(char*)machine_msg_data(msg);
		od_debug(&instance->logger, "auth", client, NULL, "%s",
		         kiwi_fe_type_to_string(type));
		if (type == KIWI_FE_PASSWORD_MESSAGE)
			break;
		machine_msg_free(msg);
	}

	/* read password message */
	kiwi_password_t client_token;
	kiwi_password_init(&client_token);

	rc = kiwi_be_read_password(machine_msg_data(msg),
	                           machine_msg_size(msg),
	                           &client_token);
	if (rc == -1) {
		od_error(&instance->logger, "auth", client, NULL,
		         "password read error");
		od_frontend_error(client, KIWI_PROTOCOL_VIOLATION,
		                  "bad password message");
		kiwi_password_free(&client_token);
		machine_msg_free(msg);
		return -1;
	}

	/* use remote or local password source */
	kiwi_password_t client_password;
	kiwi_password_init(&client_password);

	if (client->rule->auth_query) {
		rc = od_auth_query(client->global,
		                   client->rule,
		                   &client->startup.user,
		                   &client_password);
		if (rc == -1) {
			od_error(&instance->logger, "auth", client, NULL,
			         "failed to make auth_query");
			od_frontend_error(client, KIWI_INVALID_AUTHORIZATION_SPECIFICATION,
			                  "failed to make auth query");
			kiwi_password_free(&client_token);
			kiwi_password_free(&client_password);
			machine_msg_free(msg);
			return -1;
		}
	} else {
		client_password.password_len = client->rule->password_len + 1;
		client_password.password     = client->rule->password;
	}

	/* authenticate */
	int check = kiwi_password_compare(&client_password, &client_token);
	kiwi_password_free(&client_token);
	machine_msg_free(msg);

	if (client->rule->auth_query)
		kiwi_password_free(&client_password);
	if (! check) {
		od_log(&instance->logger, "auth", client, NULL,
		       "user '%s.%s' incorrect password",
		       client->startup.database.value,
		       client->startup.user.value);
		od_frontend_error(client, KIWI_INVALID_PASSWORD,
		                  "incorrect password");
		return -1;
	}
	return 0;
}

static inline int
od_auth_frontend_md5(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;

	/* generate salt */
	uint32_t salt = kiwi_password_salt(&client->key);

	/* AuthenticationMD5Password */
	machine_msg_t *msg;
	msg = kiwi_be_write_authentication_md5(NULL, (char*)&salt);
	if (msg == NULL)
		return -1;
	int rc;
	rc = od_write(&client->io, msg);
	if (rc == -1) {
		od_error(&instance->logger, "auth", client, NULL,
		         "write error: %s",
		         od_io_error(&client->io));
		return -1;
	}

	/* wait for password response */
	while (1) {
		msg = od_read(&client->io, UINT32_MAX);
		if (msg == NULL) {
			od_error(&instance->logger, "auth", client, NULL,
			         "read error: %s",
			         od_io_error(&client->io));
			return -1;
		}
		kiwi_fe_type_t type = *(char*)machine_msg_data(msg);
		od_debug(&instance->logger, "auth", client, NULL, "%s",
		         kiwi_fe_type_to_string(type));
		if (type == KIWI_FE_PASSWORD_MESSAGE)
			break;
		machine_msg_free(msg);
	}

	/* read password message */
	kiwi_password_t client_token;
	kiwi_password_init(&client_token);
	rc = kiwi_be_read_password(machine_msg_data(msg),
	                           machine_msg_size(msg),
	                           &client_token);
	if (rc == -1) {
		od_error(&instance->logger, "auth", client, NULL,
		         "password read error");
		od_frontend_error(client, KIWI_PROTOCOL_VIOLATION,
		                  "bad password message");
		kiwi_password_free(&client_token);
		machine_msg_free(msg);
		return -1;
	}

	/* use remote or local password source */
	kiwi_password_t client_password;
	kiwi_password_init(&client_password);

	kiwi_password_t query_password;
	kiwi_password_init(&query_password);

	if (client->rule->auth_query) {
		rc = od_auth_query(client->global,
		                   client->rule,
		                   &client->startup.user,
		                   &query_password);
		if (rc == -1) {
			od_error(&instance->logger, "auth", client, NULL,
			         "failed to make auth_query");
			od_frontend_error(client, KIWI_INVALID_AUTHORIZATION_SPECIFICATION,
			                  "failed to make auth query");
			kiwi_password_free(&client_token);
			kiwi_password_free(&query_password);
			machine_msg_free(msg);
			return -1;
		}
		query_password.password_len--;
	} else {
		query_password.password_len = client->rule->password_len;
		query_password.password = client->rule->password;
	}

	/* prepare password hash */
	rc = kiwi_password_md5(&client_password,
	                       client->startup.user.value,
	                       client->startup.user.value_len - 1,
	                       query_password.password,
	                       query_password.password_len,
	                       (char*)&salt);
	if (rc == -1) {
		od_error(&instance->logger, "auth", client, NULL,
		         "memory allocation error");
		kiwi_password_free(&client_password);
		kiwi_password_free(&client_token);
		if (client->rule->auth_query)
			kiwi_password_free(&query_password);
		machine_msg_free(msg);
		return -1;
	}

	/* authenticate */
	int check = kiwi_password_compare(&client_password, &client_token);
	kiwi_password_free(&client_password);
	kiwi_password_free(&client_token);
	machine_msg_free(msg);

	if (client->rule->auth_query)
		kiwi_password_free(&query_password);

	if (! check) {
		od_log(&instance->logger, "auth", client, NULL,
		       "user '%s.%s' incorrect password",
		       client->startup.database.value,
		       client->startup.user.value);
		od_frontend_error(client, KIWI_INVALID_PASSWORD,
		                  "incorrect password");
		return -1;
	}
	return 0;
}

static inline int
od_auth_frontend_cert(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;
	if (! client->startup.is_ssl_request) {
		od_error(&instance->logger, "auth", client, NULL,
		         "TLS connection required");
		od_frontend_error(client, KIWI_INVALID_AUTHORIZATION_SPECIFICATION,
		                  "TLS connection required");
		return -1;
	}

	/* compare client certificate common name */
	od_route_t *route = client->route;
	int rc;
	if (route->rule->auth_common_name_default) {
		rc = machine_io_verify(client->io.io, route->rule->user_name);
		if (! rc) {
			return 0;
		}
	}

	od_list_t *i;
	od_list_foreach(&route->rule->auth_common_names, i) {
		od_rule_auth_t *auth;
		auth = od_container_of(i, od_rule_auth_t, link);
		rc = machine_io_verify(client->io.io, auth->common_name);
		if (! rc) {
			return 0;
		}
	}

	od_error(&instance->logger, "auth", client, NULL,
	         "TLS certificate common name mismatch");
	od_frontend_error(client, KIWI_INVALID_PASSWORD,
	                  "TLS certificate common name mismatch");
	return -1;
}

static inline int
od_auth_frontend_block(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;
	od_log(&instance->logger, "auth", client, NULL,
	       "user '%s.%s' is blocked",
	       client->startup.database.value,
	       client->startup.user.value);
	od_frontend_error(client, KIWI_INVALID_AUTHORIZATION_SPECIFICATION,
	                  "user blocked");
	return 0;
}

int od_auth_frontend(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;

	/* authentication mode */
	int rc;
	switch (client->rule->auth_mode) {
	case OD_RULE_AUTH_CLEAR_TEXT:
		rc = od_auth_frontend_cleartext(client);
		if (rc == -1)
			return -1;
		break;
	case OD_RULE_AUTH_MD5:
		rc = od_auth_frontend_md5(client);
		if (rc == -1)
			return -1;
		break;
	case OD_RULE_AUTH_CERT:
		rc = od_auth_frontend_cert(client);
		if (rc == -1)
			return -1;
		break;
	case OD_RULE_AUTH_BLOCK:
		od_auth_frontend_block(client);
		return -1;
	case OD_RULE_AUTH_NONE:
		break;
	default:
		assert(0);
		break;
	}

	/* pass */
	machine_msg_t *msg;
	msg = kiwi_be_write_authentication_ok(NULL);
	if (msg == NULL)
		return -1;
	rc = od_write(&client->io, msg);
	if (rc == -1) {
		od_error(&instance->logger, "auth", client, NULL,
		         "write error: %s",
		         od_io_error(&client->io));
		return -1;
	}
	return 0;
}

static inline int
od_auth_backend_cleartext(od_server_t *server)
{
	od_instance_t *instance = server->global->instance;
	od_route_t *route = server->route;
	assert(route != NULL);

	od_debug(&instance->logger, "auth", NULL, server,
	         "requested clear-text authentication");

	/* use storage or user password */
	char *password;
	int   password_len;
	if (route->rule->storage_password) {
		password = route->rule->storage_password;
		password_len = route->rule->storage_password_len;
	} else
	if (route->rule->password) {
		password = route->rule->password;
		password_len = route->rule->password_len;
	} else {
		od_error(&instance->logger, "auth", NULL, server,
		         "password required for route '%s.%s'",
		         route->rule->db_name,
		         route->rule->user_name);
		return -1;
	}

	/* PasswordMessage */
	machine_msg_t *msg;
	msg = kiwi_fe_write_password(NULL, password, password_len + 1);
	if (msg == NULL) {
		od_error(&instance->logger, "auth", NULL, server,
		         "memory allocation error");
		return -1;
	}
	int rc;
	rc = od_write(&server->io, msg);
	if (rc == -1) {
		od_error(&instance->logger, "auth", NULL, server,
		         "write error: %s",
		         od_io_error(&server->io));
		return -1;
	}
	return 0;
}

static inline int
od_auth_backend_md5(od_server_t *server, char salt[4])
{
	od_instance_t *instance = server->global->instance;
	od_route_t *route = server->route;
	assert(route != NULL);

	od_debug(&instance->logger, "auth", NULL, server,
	         "requested md5 authentication");

	/* use storage user or route user */
	char *user;
	int   user_len;
	if (route->rule->storage_user) {
		user = route->rule->storage_user;
		user_len = route->rule->storage_user_len;
	} else {
		user = route->rule->user_name;
		user_len = route->rule->user_name_len;
	}

	/* use storage or user password */
	char *password;
	int   password_len;
	if (route->rule->storage_password) {
		password = route->rule->storage_password;
		password_len = route->rule->storage_password_len;
	} else
	if (route->rule->password) {
		password = route->rule->password;
		password_len = route->rule->password_len;
	} else {
		od_error(&instance->logger, "auth", NULL, server,
		         "password required for route '%s.%s'",
		         route->rule->db_name,
		         route->rule->user_name);
		return -1;
	}

	/* prepare md5 password using server supplied salt */
	kiwi_password_t client_password;
	kiwi_password_init(&client_password);
	int rc;
	rc = kiwi_password_md5(&client_password, user, user_len,
	                       password,
	                       password_len, salt);
	if (rc == -1) {
		od_error(&instance->logger, "auth", NULL, server,
		         "memory allocation error");
		kiwi_password_free(&client_password);
		return -1;
	}

	/* PasswordMessage */
	machine_msg_t *msg;
	msg = kiwi_fe_write_password(NULL,
	                             client_password.password,
	                             client_password.password_len);
	kiwi_password_free(&client_password);
	if (msg == NULL) {
		od_error(&instance->logger, "auth", NULL, server,
		         "memory allocation error");
		return -1;
	}
	rc = od_write(&server->io, msg);
	if (rc == -1) {
		od_error(&instance->logger, "auth", NULL, server,
		         "write error: %s",
		         od_io_error(&server->io));
		return -1;
	}
	return 0;
}

int
od_auth_backend(od_server_t *server, machine_msg_t *msg)
{
	od_instance_t *instance = server->global->instance;
	assert(*(char*)machine_msg_data(msg) == KIWI_BE_AUTHENTICATION);

	uint32_t auth_type;
	char salt[4];
	int rc;
	rc = kiwi_fe_read_auth(machine_msg_data(msg),
	                       machine_msg_size(msg),
	                       &auth_type, salt);
	if (rc == -1) {
		od_error(&instance->logger, "auth", NULL, server,
		         "failed to parse authentication message");
		return -1;
	}
	msg = NULL;

	switch (auth_type) {
	/* AuthenticationOk */
	case 0:
		return 0;
	/* AuthenticationCleartextPassword */
	case 3:
		rc = od_auth_backend_cleartext(server);
		if (rc == -1)
			return -1;
		break;
	/* AuthenticationMD5Password */
	case 5:
		rc = od_auth_backend_md5(server, salt);
		if (rc == -1)
			return -1;
		break;
	/* unsupported */
	default:
		od_error(&instance->logger, "auth", NULL, server,
		         "unsupported authentication method");
		return -1;
	}

	/* wait for authentication response */
	while (1)
	{
		msg = od_read(&server->io, UINT32_MAX);
		if (rc == -1) {
			od_error(&instance->logger, "auth", NULL, server,
			         "read error: %s",
			         od_io_error(&server->io));
			return -1;
		}
		kiwi_be_type_t type = *(char*)machine_msg_data(msg);
		od_debug(&instance->logger, "auth", NULL, server, "%s",
		         kiwi_be_type_to_string(type));

		int rc;
		switch (type) {
		case KIWI_BE_AUTHENTICATION:
			rc = kiwi_fe_read_auth(machine_msg_data(msg),
			                       machine_msg_size(msg),
			                       &auth_type, salt);
			machine_msg_free(msg);
			if (rc == -1) {
				od_error(&instance->logger, "auth", NULL, server,
				         "failed to parse authentication message");
				return -1;
			}
			if (auth_type != 0) {
				od_error(&instance->logger, "auth", NULL, server,
				         "incorrect authentication flow");
				return 0;
			}
			return 0;
		case KIWI_BE_ERROR_RESPONSE:
			od_backend_error(server, "auth", machine_msg_data(msg),
			                 machine_msg_size(msg));
			machine_msg_free(msg);
			return -1;
		default:
			machine_msg_free(msg);
			break;
		}
	}
	return 0;
}
