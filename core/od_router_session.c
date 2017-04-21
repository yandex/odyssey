
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
#include "od_stat.h"
#include "od_server.h"
#include "od_server_pool.h"
#include "od_client.h"
#include "od_client_list.h"
#include "od_client_pool.h"
#include "od_route_id.h"
#include "od_route.h"
#include "od_route_pool.h"
#include "od.h"
#include "od_io.h"
#include "od_pooler.h"
#include "od_router.h"
#include "od_router_session.h"
#include "od_router_copy.h"
#include "od_cancel.h"
#include "od_fe.h"
#include "od_be.h"

od_routerstatus_t
od_router_session(od_client_t *client)
{
	od_pooler_t *pooler = client->pooler;
	int rc, type;

	/* client routing */
	od_route_t *route = od_route(pooler, &client->startup);
	if (route == NULL) {
		od_error(&pooler->od->log, client->io,
		         "C: database route '%s' is not declared",
		         so_parameter_value(client->startup.database));
		return OD_RS_EROUTE;
	}
	/* ensure client_max limit per route */
	int client_total = od_clientpool_total(&route->client_pool);
	if (client_total >= route->scheme->client_max) {
		od_log(&pooler->od->log, client->io,
		       "C: route '%s' client_max reached (%d), closing connection",
		       route->scheme->target,
		       route->scheme->client_max);
		return OD_RS_ELIMIT;
	}
	client->route = route;
	od_clientpool_set(&route->client_pool, client, OD_CPENDING);

	/* get server connection for the route */
	od_server_t *server = od_bepop(pooler, route, client);
	if (server == NULL)
		return OD_RS_EPOOL;
	client->server = server;

	/* configure server using client startup parameters */
	rc = od_beconfigure(server, &client->startup);
	if (rc == -1)
		return OD_RS_ESERVER_CONFIGURE;

	/* assign client session key */
	server->key_client = client->key;

	od_debug(&pooler->od->log, client->io,
	         "C: route to '%s' (using '%s' server)",
	         route->scheme->target,
	         route->scheme->server->name);

	so_stream_t *stream = &client->stream;
	for (;;)
	{
		/* client to server */
		so_stream_reset(stream);
		rc = od_read(client->io, stream, INT_MAX);
		if (rc == -1)
			return OD_RS_ECLIENT_READ;
		type = stream->s[rc];
		od_debug(&pooler->od->log, client->io, "C: %c", type);

		/* client graceful shutdown */
		if (type == 'X')
			break;

		rc = od_write(server->io, stream);
		if (rc == -1)
			return OD_RS_ESERVER_WRITE;

		server->count_request++;

		so_stream_reset(stream);
		for (;;) {
			/* pipeline server reply */
			for (;;) {
				rc = od_read(server->io, stream, 1000);
				if (rc >= 0)
					break;
				/* client watchdog.
				 *
				 * ensure that client has not closed
				 * the connection */
				if (! machine_read_timedout(server->io))
					return OD_RS_ESERVER_READ;
				if (machine_connected(client->io))
					continue;
				od_debug(&pooler->od->log, server->io,
				         "S (watchdog): client disconnected");
				return OD_RS_ECLIENT_READ;
			}
			type = stream->s[rc];
			od_debug(&pooler->od->log, server->io, "S: %c", type);

			/* ReadyForQuery */
			if (type == 'Z') {
				rc = od_beset_ready(server, stream->s + rc,
				                    so_stream_used(stream) - rc);
				if (rc == -1)
					return OD_RS_ECLIENT_READ;

				/* flush reply buffer to client */
				rc = od_write(client->io, stream);
				if (rc == -1)
					return OD_RS_ECLIENT_WRITE;

				break;
			}

			/* CopyInResponse */
			if (type == 'G') {
				/* transmit reply to client */
				rc = od_write(client->io, stream);
				if (rc == -1)
					return OD_RS_ECLIENT_WRITE;
				od_routerstatus_t copy_rc;
				copy_rc = od_router_copy_in(client);
				if (copy_rc != OD_RS_OK)
					return copy_rc;
				continue;
			}
			/* CopyOutResponse */
			if (type == 'H') {
				assert(! server->is_copy);
				server->is_copy = 1;
				continue;
			}
			/* copy out complete */
			if (type == 'c') {
				server->is_copy = 0;
				continue;
			}
		}
	}

	return OD_RS_OK;
}
