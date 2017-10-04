
/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <signal.h>

#include <machinarium.h>
#include <shapito.h>

#include "sources/macro.h"
#include "sources/version.h"
#include "sources/error.h"
#include "sources/atomic.h"
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/logger.h"
#include "sources/daemon.h"
#include "sources/scheme.h"
#include "sources/scheme_mgr.h"
#include "sources/config.h"
#include "sources/msg.h"
#include "sources/system.h"
#include "sources/server.h"
#include "sources/server_pool.h"
#include "sources/client.h"
#include "sources/client_pool.h"
#include "sources/route_id.h"
#include "sources/route.h"
#include "sources/route_pool.h"
#include "sources/io.h"
#include "sources/instance.h"
#include "sources/router.h"
#include "sources/pooler.h"
#include "sources/relay.h"
#include "sources/frontend.h"
#include "sources/backend.h"
#include "sources/auth.h"
#include "sources/auth_query.h"

static inline int
od_auth_frontend_cleartext(od_client_t *client)
{
	od_instance_t *instance = client->system->instance;

	/* AuthenticationCleartextPassword */
	shapito_stream_t *stream = &client->stream;
	shapito_stream_reset(stream);
	int rc;
	rc = shapito_be_write_authentication_clear_text(stream);
	if (rc == -1)
		return -1;
	rc = od_write(client->io, stream);
	if (rc == -1) {
		od_error(&instance->logger, "auth", client, NULL,
		         "write error: %s",
		         machine_error(client->io));
		return -1;
	}

	/* wait for password response */
	while (1) {
		shapito_stream_reset(stream);
		rc = od_read(client->io, stream, UINT32_MAX);
		if (rc == -1) {
			od_error(&instance->logger, "auth", client, NULL,
			         "read error: %s",
			         machine_error(client->io));
			return -1;
		}
		char type = *stream->start;
		od_debug(&instance->logger, "auth", client, NULL,
		         "%c", type);
		/* PasswordMessage */
		if (type == 'p')
			break;
	}

	/* read password message */
	shapito_password_t client_token;
	shapito_password_init(&client_token);
	rc = shapito_be_read_password(&client_token, stream->start,
	                              shapito_stream_used(stream));
	if (rc == -1) {
		od_error(&instance->logger, "auth", client, NULL,
		         "password read error");
		od_frontend_error(client, SHAPITO_PROTOCOL_VIOLATION,
		                  "bad password message");
		shapito_password_free(&client_token);
		return -1;
	}

	/* use remote or local password source */
	shapito_password_t client_password;
	shapito_password_init(&client_password);

	if (client->scheme->auth_query) {
		rc = od_auth_query(client->system, client->scheme, &client_password);
		if (rc == -1) {
			od_error(&instance->logger, "auth", client, NULL,
			         "failed to make auth_query");
			od_frontend_error(client, SHAPITO_INVALID_AUTHORIZATION_SPECIFICATION,
			                  "failed to make auth query");
			shapito_password_free(&client_token);
			shapito_password_free(&client_password);
			return -1;
		}
	} else {
		client_password.password_len = client->scheme->password_len + 1;
		client_password.password     = client->scheme->password;
	}

	/* authenticate */
	int check = shapito_password_compare(&client_password, &client_token);
	shapito_password_free(&client_token);
	if (client->scheme->auth_query)
		shapito_password_free(&client_password);
	if (! check) {
		od_log(&instance->logger, "auth", client, NULL,
		       "user '%s.%s' incorrect password",
		       shapito_parameter_value(client->startup.database),
		       shapito_parameter_value(client->startup.user));
		od_frontend_error(client, SHAPITO_INVALID_PASSWORD,
		                  "incorrect password");
		return -1;
	}
	return 0;
}

static inline int
od_auth_frontend_md5(od_client_t *client)
{
	od_instance_t *instance = client->system->instance;

	/* generate salt */
	uint32_t salt = shapito_password_salt(&client->key);

	/* AuthenticationMD5Password */
	shapito_stream_t *stream = &client->stream;
	shapito_stream_reset(stream);
	int rc;
	rc = shapito_be_write_authentication_md5(stream, (char*)&salt);
	if (rc == -1)
		return -1;
	rc = od_write(client->io, stream);
	if (rc == -1) {
		od_error(&instance->logger, "auth", client, NULL,
		         "write error: %s",
		         machine_error(client->io));
		return -1;
	}

	/* wait for password response */
	while (1) {
		int rc;
		shapito_stream_reset(stream);
		rc = od_read(client->io, stream, UINT32_MAX);
		if (rc == -1) {
			od_error(&instance->logger, "auth", client, NULL,
			         "read error: %s",
			         machine_error(client->io));
			return -1;
		}
		char type = *stream->start;
		od_debug(&instance->logger, "auth", client, NULL,
		         "%c", type);
		/* PasswordMessage */
		if (type == 'p')
			break;
	}

	/* read password message */
	shapito_password_t client_token;
	shapito_password_init(&client_token);
	rc = shapito_be_read_password(&client_token, stream->start,
	                              shapito_stream_used(stream));
	if (rc == -1) {
		od_error(&instance->logger, "auth", client, NULL,
		         "password read error");
		od_frontend_error(client, SHAPITO_PROTOCOL_VIOLATION,
		                  "bad password message");
		shapito_password_free(&client_token);
		return -1;
	}

	/* use remote or local password source */
	shapito_password_t client_password;
	shapito_password_init(&client_password);

	shapito_password_t query_password;
	shapito_password_init(&query_password);

	if (client->scheme->auth_query) {
		rc = od_auth_query(client->system, client->scheme, &query_password);
		if (rc == -1) {
			od_error(&instance->logger, "auth", client, NULL,
			         "failed to make auth_query");
			od_frontend_error(client, SHAPITO_INVALID_AUTHORIZATION_SPECIFICATION,
			                  "failed to make auth query");
			shapito_password_free(&client_token);
			shapito_password_free(&query_password);
			return -1;
		}
	} else {
		query_password.password_len = client->scheme->password_len;
		query_password.password = client->scheme->password;
	}

	/* prepare password hash */
	rc = shapito_password_md5(&client_password,
	                          shapito_parameter_value(client->startup.user),
	                          client->startup.user->value_len - 1,
	                          query_password.password,
	                          query_password.password_len,
	                          (char*)&salt);
	if (rc == -1) {
		od_error(&instance->logger, "auth", client, NULL,
		         "memory allocation error");
		shapito_password_free(&client_password);
		shapito_password_free(&client_token);
		if (client->scheme->auth_query)
			shapito_password_free(&query_password);
		return -1;
	}

	/* authenticate */
	int check = shapito_password_compare(&client_password, &client_token);
	shapito_password_free(&client_password);
	shapito_password_free(&client_token);
	if (client->scheme->auth_query)
		shapito_password_free(&query_password);
	if (! check) {
		od_log(&instance->logger, "auth", client, NULL,
		       "user '%s.%s' incorrect password",
		       shapito_parameter_value(client->startup.database),
		       shapito_parameter_value(client->startup.user));
		od_frontend_error(client, SHAPITO_INVALID_PASSWORD,
		                  "incorrect password");
		return -1;
	}
	return 0;
}

static inline int
od_auth_frontend_block(od_client_t *client)
{
	od_instance_t *instance = client->system->instance;
	od_log(&instance->logger, "auth", client, NULL,
	       "user '%s.%s' is blocked",
	       shapito_parameter_value(client->startup.database),
	       shapito_parameter_value(client->startup.user));
	od_frontend_error(client, SHAPITO_INVALID_AUTHORIZATION_SPECIFICATION,
	                  "user blocked");
	return 0;
}

int od_auth_frontend(od_client_t *client)
{
	od_instance_t *instance = client->system->instance;

	/* authentication mode */
	int rc;
	switch (client->scheme->auth_mode) {
	case OD_AUTH_CLEAR_TEXT:
		rc = od_auth_frontend_cleartext(client);
		if (rc == -1)
			return -1;
		break;
	case OD_AUTH_MD5:
		rc = od_auth_frontend_md5(client);
		if (rc == -1)
			return -1;
		break;
	case OD_AUTH_BLOCK:
		od_auth_frontend_block(client);
		return -1;
	case OD_AUTH_NONE:
		break;
	default:
		assert(0);
		break;
	}

	/* pass */
	shapito_stream_t *stream = &client->stream;
	shapito_stream_reset(stream);
	rc = shapito_be_write_authentication_ok(stream);
	if (rc == -1)
		return -1;
	rc = od_write(client->io, stream);
	if (rc == -1) {
		od_error(&instance->logger, "auth", client, NULL,
		         "write error: %s",
		         machine_error(client->io));
		return -1;
	}
	return 0;
}

static inline int
od_auth_backend_cleartext(od_server_t *server)
{
	od_instance_t *instance = server->system->instance;
	od_route_t *route = server->route;
	assert(route != NULL);

	od_debug(&instance->logger, "auth", NULL, server,
	         "requested clear-text authentication");

	/* use storage or user password */
	char *password;
	int   password_len;
	if (route->scheme->storage_password) {
		password = route->scheme->storage_password;
		password_len = route->scheme->storage_password_len;
	} else
	if (route->scheme->password) {
		password = route->scheme->password;
		password_len = route->scheme->password_len;
	} else {
		od_error(&instance->logger, "auth", NULL, server,
		         "password required for route '%s.%s'",
		         route->scheme->db_name,
		         route->scheme->user_name);
		return -1;
	}

	/* PasswordMessage */
	shapito_stream_t *stream = &server->stream;
	shapito_stream_reset(stream);
	int rc;
	rc = shapito_fe_write_password(stream, password, password_len + 1);
	if (rc == -1) {
		od_error(&instance->logger, "auth", NULL, server,
		         "memory allocation error");
		return -1;
	}
	rc = od_write(server->io, stream);
	if (rc == -1) {
		od_error(&instance->logger, "auth", NULL, server,
		         "write error: %s",
		         machine_error(server->io));
		return -1;
	}
	return 0;
}

static inline int
od_auth_backend_md5(od_server_t *server, char salt[4])
{
	od_instance_t *instance = server->system->instance;
	od_route_t *route = server->route;
	assert(route != NULL);

	od_debug(&instance->logger, "auth", NULL, server,
	         "requested md5 authentication");

	/* use storage user or route user */
	char *user;
	int   user_len;
	if (route->scheme->storage_user) {
		user = route->scheme->storage_user;
		user_len = route->scheme->storage_user_len;
	} else {
		user = route->scheme->user_name;
		user_len = route->scheme->user_name_len;
	}

	/* use storage or user password */
	char *password;
	int   password_len;
	if (route->scheme->storage_password) {
		password = route->scheme->storage_password;
		password_len = route->scheme->storage_password_len;
	} else
	if (route->scheme->password) {
		password = route->scheme->password;
		password_len = route->scheme->password_len;
	} else {
		od_error(&instance->logger, "auth", NULL, server,
		         "password required for route '%s.%s'",
		         route->scheme->db_name,
		         route->scheme->user_name);
		return -1;
	}

	/* prepare md5 password using server supplied salt */
	shapito_password_t client_password;
	shapito_password_init(&client_password);
	int rc;
	rc = shapito_password_md5(&client_password, user, user_len,
	                          password,
	                          password_len, salt);
	if (rc == -1) {
		od_error(&instance->logger, "auth", NULL, server,
		         "memory allocation error");
		shapito_password_free(&client_password);
		return -1;
	}

	/* PasswordMessage */
	shapito_stream_t *stream = &server->stream;
	shapito_stream_reset(stream);
	rc = shapito_fe_write_password(stream,
	                               client_password.password,
	                               client_password.password_len);
	shapito_password_free(&client_password);
	if (rc == -1) {
		od_error(&instance->logger, "auth", NULL, server,
		         "memory allocation error");
		return -1;
	}
	rc = od_write(server->io, stream);
	if (rc == -1) {
		od_error(&instance->logger, "auth", NULL, server,
		         "write error: %s",
		         machine_error(server->io));
		return -1;
	}
	return 0;
}

int od_auth_backend(od_server_t *server)
{
	od_instance_t *instance = server->system->instance;

	shapito_stream_t *stream = &server->stream;
	assert(*stream->start == 'R');

	uint32_t auth_type;
	char salt[4];
	int rc;
	rc = shapito_fe_read_auth(&auth_type, salt, stream->start,
	                          shapito_stream_used(stream));
	if (rc == -1) {
		od_error(&instance->logger, "auth", NULL, server,
		         "failed to parse authentication message");
		return -1;
	}
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
	while (1) {
		int rc;
		shapito_stream_reset(stream);
		rc = od_read(server->io, &server->stream, UINT32_MAX);
		if (rc == -1) {
			od_error(&instance->logger, "auth", NULL, server,
			         "read error: %s",
			         machine_error(server->io));
			return -1;
		}
		char type = *server->stream.start;
		od_debug(&instance->logger, "auth", NULL, server,
		         "%c", type);
		switch (type) {
		case 'R':
			rc = shapito_fe_read_auth(&auth_type, salt, stream->start,
			                          shapito_stream_used(stream));
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
		case 'E':
			od_backend_error(server, "auth", stream->start,
			                 shapito_stream_used(stream));
			return -1;
		}
	}
	return 0;
}
