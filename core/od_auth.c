
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

int od_auth(od_client_t *client)
{
	od_pooler_t *pooler = client->pooler;

	/* match user scheme */
	od_schemeuser_t *user_scheme;
	user_scheme =
		od_schemeuser_match(&pooler->od->scheme, client->startup.user);
	if (user_scheme == NULL) {
		/* try to use default user */
		user_scheme = pooler->od->scheme.users_default;
		if (user_scheme == NULL) {
			od_error(&pooler->od->log, client->io,
			         "C: user '%s' scheme is not declared",
			          client->startup.user);
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

	so_stream_t *stream = &client->stream;
	so_stream_reset(stream);
	int rc;
	rc = so_bewrite_authentication(stream, 0);
	if (rc == -1)
		return -1;
	rc = od_write(client->io, stream);
	return rc;
}
