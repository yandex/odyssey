
/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <machinarium.h>
#include <soprano.h>

#include "od_macro.h"
#include "od_list.h"
#include "od_pid.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"
#include "od_server.h"
#include "od_server_pool.h"
#include "od_route_id.h"
#include "od_route.h"
#include "od_route_pool.h"
#include "od_client.h"
#include "od_client_pool.h"
#include "od.h"
#include "od_io.h"
#include "od_pooler.h"
#include "od_fe.h"
#include "od_auth.h"

static inline int
od_auth_cleartext(od_client_t *client)
{
	od_pooler_t *pooler = client->pooler;

	/* AuthenticationCleartextPassword */
	so_stream_t *stream = &client->stream;
	so_stream_reset(stream);
	int rc;
	rc = so_bewrite_authentication_clear_text(stream);
	if (rc == -1)
		return -1;
	rc = od_write(client->io, stream);
	if (rc == -1)
		return -1;

	/* wait for password response */
	while (1) {
		rc = od_read(client->io, stream, 0);
		if (rc == -1)
			return -1;
		uint8_t type = *stream->s;
		od_debug(&pooler->od->log, client->io, "C (auth): %c", *stream->s);
		/* PasswordMessage */
		if (type == 'p')
			break;
	}

	/* read password message */
	so_password_t client_token;
	so_password_init(&client_token);
	rc = so_beread_password(&client_token, stream->s,
	                        so_stream_used(stream));
	if (rc == -1) {
		od_error(&pooler->od->log, client->io,
		         "C: password read error");
		so_password_free(&client_token);
		return -1;
	}

	/* set user password */
	so_password_t client_password = {
		.password_len = client->scheme->password_len + 1,
		.password     = client->scheme->password,
	};

	/* authenticate */
	int check = so_password_compare(&client_password, &client_token);
	so_password_free(&client_token);
	if (! check) {
		od_log(&pooler->od->log, client->io,
		       "C: user '%s' incorrect password",
		        client->startup.user);
		return -1;
	}
	return 0;
}

static inline int
od_auth_md5(od_client_t *client)
{
	od_pooler_t *pooler = client->pooler;

	/* generate salt */
	uint32_t salt = so_password_salt(&client->key);

	/* AuthenticationMD5Password */
	so_stream_t *stream = &client->stream;
	so_stream_reset(stream);
	int rc;
	rc = so_bewrite_authentication_md5(stream, (uint8_t*)&salt);
	if (rc == -1)
		return -1;
	rc = od_write(client->io, stream);
	if (rc == -1)
		return -1;

	/* wait for password response */
	while (1) {
		int rc;
		rc = od_read(client->io, stream, 0);
		if (rc == -1)
			return -1;
		uint8_t type = *stream->s;
		od_debug(&pooler->od->log, client->io, "C (auth): %c", *stream->s);
		/* PasswordMessage */
		if (type == 'p')
			break;
	}

	/* read password message */
	so_password_t client_token;
	so_password_init(&client_token);
	rc = so_beread_password(&client_token, stream->s,
	                        so_stream_used(stream));
	if (rc == -1) {
		od_error(&pooler->od->log, client->io,
		         "C: password read error");
		so_password_free(&client_token);
		return -1;
	}

	/* set user password */
	so_password_t client_password;
	so_password_init(&client_password);
	rc = so_password_md5(&client_password,
	                     client->startup.user,
	                     client->startup.user_len - 1,
	                     client->scheme->password,
	                     client->scheme->password_len,
	                     (uint8_t*)&salt);
	if (rc == -1) {
		od_error(&pooler->od->log, NULL, "memory allocation error");
		so_password_free(&client_password);
		so_password_free(&client_token);
		return -1;
	}

	/* authenticate */
	int check = so_password_compare(&client_password, &client_token);
	so_password_free(&client_password);
	so_password_free(&client_token);
	if (! check) {
		od_log(&pooler->od->log, client->io,
		       "C: user '%s' incorrect password",
		        client->startup.user);
		return -1;
	}
	return 0;
}

int od_auth(od_client_t *client)
{
	od_pooler_t *pooler = client->pooler;

	/* match user scheme */
	od_schemeuser_t *user_scheme =
		od_schemeuser_match(&pooler->od->scheme, client->startup.user);
	if (user_scheme == NULL) {
		/* try to use default user */
		user_scheme = pooler->od->scheme.users_default;
		if (user_scheme == NULL) {
			od_error(&pooler->od->log, client->io,
			         "C: user '%s' not found", client->startup.user);
			return -1;
		}
	}
	client->scheme = user_scheme;

	/* is user access denied */
	if (user_scheme->is_deny) {
		od_log(&pooler->od->log, client->io,
		       "C: user '%s' access denied", client->startup.user);
		return -1;
	}

	/* authentication mode */
	int rc;
	switch (user_scheme->auth_mode) {
	case OD_ACLEAR_TEXT:
		rc = od_auth_cleartext(client);
		if (rc == -1)
			return -1;
		break;
	case OD_AMD5:
		rc = od_auth_md5(client);
		if (rc == -1)
			return -1;
		break;
	case OD_ANONE:
		break;
	default:
		assert(0);
		break;
	}

	/* pass */
	so_stream_t *stream = &client->stream;
	so_stream_reset(stream);
	rc = so_bewrite_authentication_ok(stream);
	if (rc == -1)
		return -1;
	rc = od_write(client->io, stream);
	return rc;
}

static inline int
od_authbe_cleartext(od_server_t *server)
{
	od_pooler_t *pooler = server->pooler;

	od_route_t *route = server->route;
	assert(route != NULL);
	if (route->scheme->password == NULL) {
		od_error(&pooler->od->log, server->io,
		         "S: password required for route '%s'",
		          route->scheme->target);
		return -1;
	}

	/* PasswordMessage */
	so_stream_t *stream = &server->stream;
	so_stream_reset(stream);
	int rc;
	rc = so_fewrite_password(stream,
	                         route->scheme->password,
	                         route->scheme->password_len + 1);
	if (rc == -1) {
		od_error(&pooler->od->log, NULL, "memory allocation error");
		return -1;
	}
	rc = od_write(server->io, stream);
	if (rc == -1) {
		return -1;
	}
	return 0;
}

int od_authbe(od_server_t *server)
{
	od_pooler_t *pooler = server->pooler;

	so_stream_t *stream = &server->stream;
	assert(*stream->s == 'R');

	uint32_t auth_type;
	uint8_t  salt[4];
	int rc;
	rc = so_feread_auth(&auth_type, salt, stream->s,
	                    so_stream_used(stream));
	if (rc == -1) {
		od_error(&pooler->od->log, server->io,
		         "S: failed to parse authentication message");
		return -1;
	}
	switch (auth_type) {
	/* AuthenticationOk */
	case 0:
		return 0;
	/* AuthenticationCleartextPassword */
	case 3:
		rc = od_authbe_cleartext(server);
		if (rc == -1)
			return -1;
		break;
	/* AuthenticationMD5Password */
	case 5:
		(void)salt;
		break;
	/* unsupported */
	default:
		od_error(&pooler->od->log, server->io,
		         "S: unuspported authentication method");
		return -1;
	}

	/* wait for authentication response */
	while (1) {
		int rc;
		rc = od_read(server->io, &server->stream, 0);
		if (rc == -1)
			return -1;
		char type = *server->stream.s;
		od_debug(&pooler->od->log, server->io, "S (auth): %c",
		         type);
		switch (type) {
		case 'R': {
			rc = so_feread_auth(&auth_type, salt, stream->s,
			                    so_stream_used(stream));
			if (rc == -1) {
				od_error(&pooler->od->log, server->io,
		                 "S: failed to parse authentication message");
				return -1;
			}
			if (auth_type != 0) {
				od_error(&pooler->od->log, server->io,
				        "S: incorrect authentication flow");
				return 0;
			}
			return 0;
		}
		case 'E':
			od_error(&pooler->od->log, server->io,
			         "S: authentication error");
			return -1;
		}
	}
	return 0;
}
