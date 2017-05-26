
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
#include <signal.h>

#include <machinarium.h>
#include <soprano.h>

#include "od_macro.h"
#include "od_version.h"
#include "od_list.h"
#include "od_pid.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_daemon.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"
#include "od_msg.h"
#include "od_system.h"
#include "od_instance.h"

#include "od_server.h"
#include "od_server_pool.h"
#include "od_client.h"
#include "od_client_pool.h"
#include "od_route_id.h"
#include "od_route.h"
#include "od_io.h"

#include "od_pooler.h"
#include "od_relay.h"
#include "od_frontend.h"
#include "od_auth.h"

static inline int
od_auth_frontend_cleartext(od_client_t *client)
{
	od_relay_t *relay = client->relay;
	od_instance_t *instance = relay->system->instance;

	/* AuthenticationCleartextPassword */
	so_stream_t *stream = &client->stream;
	so_stream_reset(stream);
	int rc;
	rc = so_bewrite_authentication_clear_text(stream);
	if (rc == -1)
		return -1;
	rc = od_write(client->io, stream);
	if (rc == -1) {
		od_error(&instance->log, client->io, "C (auth): write error: %s",
		         machine_error(client->io));
		return -1;
	}

	/* wait for password response */
	while (1) {
		so_stream_reset(stream);
		rc = od_read(client->io, stream, INT_MAX);
		if (rc == -1) {
			od_error(&instance->log, client->io, "C (auth): read error: %s",
			         machine_error(client->io));
			return -1;
		}
		uint8_t type = *stream->s;
		od_debug(&instance->log, client->io, "C (auth): %c", *stream->s);
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
		od_error(&instance->log, client->io,
		         "C (auth): password read error");
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
		od_log(&instance->log, client->io,
		       "C (auth): user '%s' incorrect password",
		        client->startup.user);
		return -1;
	}
	return 0;
}

static inline int
od_auth_frontend_md5(od_client_t *client)
{
	od_relay_t *relay = client->relay;
	od_instance_t *instance = relay->system->instance;

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
	if (rc == -1) {
		od_error(&instance->log, client->io, "C (auth): write error: %s",
		         machine_error(client->io));
		return -1;
	}

	/* wait for password response */
	while (1) {
		int rc;
		so_stream_reset(stream);
		rc = od_read(client->io, stream, INT_MAX);
		if (rc == -1) {
			od_error(&instance->log, client->io, "C (auth): read error: %s",
			         machine_error(client->io));
			return -1;
		}
		uint8_t type = *stream->s;
		od_debug(&instance->log, client->io, "C (auth): %c", *stream->s);
		/* PasswordMessage */
		if (type == 'p')
			break;
	}

	/* read password message */
	so_password_t client_token;
	so_password_init(&client_token);
	rc = so_beread_password(&client_token, stream->s, so_stream_used(stream));
	if (rc == -1) {
		od_error(&instance->log, client->io,
		         "C (auth): password read error");
		so_password_free(&client_token);
		return -1;
	}

	/* set user password */
	so_password_t client_password;
	so_password_init(&client_password);
	rc = so_password_md5(&client_password,
	                     so_parameter_value(client->startup.user),
	                     client->startup.user->value_len - 1,
	                     client->scheme->password,
	                     client->scheme->password_len,
	                     (uint8_t*)&salt);
	if (rc == -1) {
		od_error(&instance->log, NULL, "memory allocation error");
		so_password_free(&client_password);
		so_password_free(&client_token);
		return -1;
	}

	/* authenticate */
	int check = so_password_compare(&client_password, &client_token);
	so_password_free(&client_password);
	so_password_free(&client_token);
	if (! check) {
		od_log(&instance->log, client->io,
		       "C (auth): user '%s' incorrect password",
		        client->startup.user);
		return -1;
	}
	return 0;
}

int od_auth_frontend(od_client_t *client)
{
	od_relay_t *relay = client->relay;
	od_instance_t *instance = relay->system->instance;

	/* match user scheme */
	od_schemeuser_t *user_scheme =
		od_schemeuser_match(&instance->scheme,
		                    so_parameter_value(client->startup.user));
	if (user_scheme == NULL) {
		/* try to use default user */
		user_scheme = instance->scheme.users_default;
		if (user_scheme == NULL) {
			od_error(&instance->log, client->io,
			         "C (auth): user '%s' not found",
			         so_parameter_value(client->startup.user));
			return -1;
		}
	}
	client->scheme = user_scheme;

	/* is user access denied */
	if (user_scheme->is_deny) {
		od_log(&instance->log, client->io,
		       "C (auth): user '%s' access denied",
		       so_parameter_value(client->startup.user));
		return -1;
	}

	/* authentication mode */
	int rc;
	switch (user_scheme->auth_mode) {
	case OD_ACLEAR_TEXT:
		rc = od_auth_frontend_cleartext(client);
		if (rc == -1)
			return -1;
		break;
	case OD_AMD5:
		rc = od_auth_frontend_md5(client);
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
	if (rc == -1) {
		od_error(&instance->log, client->io, "C (auth): write error: %s",
		         machine_error(client->io));
		return -1;
	}
	return 0;
}
