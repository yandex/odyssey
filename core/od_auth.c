
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
od_auth_request(od_client_t *client, int type)
{
	so_stream_t *stream = &client->stream;
	so_stream_reset(stream);
	int rc;
	rc = so_bewrite_authentication(stream, type);
	if (rc == -1)
		return -1;
	rc = od_write(client->io, stream);
	return rc;
}

static inline int
od_auth_cleartext(od_client_t *client)
{
	od_pooler_t *pooler = client->pooler;

	/* AuthenticationCleartextPassword */
	int rc = od_auth_request(client, 3);
	if (rc == -1)
		return -1;

	/* wait for password response */
	so_stream_t *stream = &client->stream;
	while (1) {
		int rc;
		rc = od_read(client->io, stream, 0);
		if (rc == -1)
			return -1;
		uint8_t type = *stream->s;
		od_debug(&pooler->od->log, client->io, "C: %c", *stream->s);
		/* PasswordMessage */
		if (type == 'p')
			break;
	}

	/* read password message */
	so_bepassword_t pw;
	so_bepassword_init(&pw);
	rc = so_beread_password(&pw, stream->s, so_stream_used(stream));
	if (rc == -1) {
		od_error(&pooler->od->log, client->io,
		         "C: password read error");
		so_bepassword_free(&pw);
		return -1;
	}

	/* check user password */
	int client_password_len = client->user->password_len + 1;
	int check = (client_password_len == pw.password_len) &&
	            (memcmp(client->user->password, pw.password,
	                    client_password_len) == 0);
	so_bepassword_free(&pw);
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
	client->user = user_scheme;

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
		break;
	case OD_ANONE:
		break;
	default:
		assert(0);
		break;
	}

	/* authentication ok */
	rc = od_auth_request(client, 0);
	return rc;
}
