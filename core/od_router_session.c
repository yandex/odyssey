
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
#include "od_router.h"
#include "od_router_session.h"
#include "od_link.h"
#include "od_cancel.h"
#include "od_fe.h"
#include "od_be.h"

static void
od_router_session_relay(void *arg)
{
	od_link_t   *link   = arg;
	od_client_t *client = link->client;
	od_server_t *server = link->server;
	od_route_t  *route  = server->route;
	od_pooler_t *pooler = server->pooler;

	od_debug(&pooler->od->log, "S: server '%s' relay started",
	         route->scheme->server->name);

	so_stream_t *stream = &server->stream;
	int rc, type;
	for (;;)
	{
		/* read reply from server */
		rc = od_read(server->io, stream, 0);
		if (od_link_isbroken(link))
			break;
		if (rc == -1) {
			od_link_break(link, OD_RS_ESERVER_READ);
			break;
		}

		type = *stream->s;
		od_debug(&pooler->od->log, "S: %c", type);

		if (type == 'Z')
			od_beset_ready(server, stream);

		/* transmit reply to client */
		rc = od_write(client->io, stream);
		if (od_link_isbroken(link))
			break;
		if (rc == -1) {
			od_link_break(link, OD_RS_ECLIENT_WRITE);
			break;
		}
	}
	link->server_is_active = 0;

	od_debug(&pooler->od->log, "S: server '%s' relay stopped",
	         route->scheme->server->name);
}

od_routerstatus_t
od_router_session(od_client_t *client)
{
	od_pooler_t *pooler = client->pooler;
	int rc, type;

	/* client routing */
	od_route_t *route = od_route(pooler, &client->startup);
	if (route == NULL) {
		od_error(&pooler->od->log, "C: database route '%s' is not declared",
		         client->startup.database);
		return OD_RS_EROUTE;
	}
	/* get server connection for the route */
	od_server_t *server = od_bepop(pooler, route);
	if (server == NULL)
		return OD_RS_EPOOL;

	/* assign client session key */
	server->key_client = client->key;
	client->server = server;

	od_debug(&pooler->od->log, "C: route to %s server",
	         route->scheme->server->name);

	od_link_t link;
	od_linkinit(&link, client, server);

	/* create server relay fiber */
	int relay_id;
	relay_id = mm_create(pooler->env, od_router_session_relay, &link);
	if (relay_id < 0)
		return OD_RS_ESERVER_READ;

	so_stream_t *stream = &client->stream;
	for (;;)
	{
		/* client to server */
		rc = od_read(client->io, stream, 0);
		if (od_link_isbroken(&link))
			break;
		if (rc == -1) {
			od_link_break(&link, OD_RS_ECLIENT_READ);
			break;
		}

		type = *stream->s;
		od_debug(&pooler->od->log, "C: %c", *stream->s);

		/* client graceful shutdown */
		if (type == 'X') {
			od_link_break(&link, OD_RS_OK);
			break;
		}

		rc = od_write(server->io, stream);
		if (od_link_isbroken(&link))
			break;
		if (rc == -1) {
			od_link_break(&link, OD_RS_ESERVER_WRITE);
			break;
		}
		server->count_request++;
	}

	/* stop server relay and wait for its completion */
	if (link.server_is_active) {
		rc = mm_cancel(pooler->env, relay_id);
		assert(rc == 0);
	}
	rc = mm_wait(pooler->env, relay_id);
	assert(rc == 0);

	return link.rc;
}
