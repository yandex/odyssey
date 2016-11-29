
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
#include "od_link.h"
#include "od_cancel.h"
#include "od_fe.h"
#include "od_be.h"

static od_route_t*
od_route(od_pooler_t *pooler, so_bestartup_t *startup)
{
	assert(startup->database != NULL);
	assert(startup->user != NULL);

	/* match required route according to scheme */
	od_schemeroute_t *route_scheme;
	route_scheme =
		od_schemeroute_match(&pooler->od->scheme, startup->database);
	if (route_scheme == NULL) {
		/* try to use default route */
		route_scheme = pooler->od->scheme.routing_default;
		if (route_scheme == NULL)
			return NULL;
	}
	od_routeid_t id = {
		.database     = startup->database,
		.database_len = startup->database_len,
		.user         = startup->user,
		.user_len     = startup->user_len
	};

	/* force settings required by route */
	if (route_scheme->database) {
		id.database = route_scheme->database;
		id.database_len = strlen(route_scheme->database) + 1;
	}
	if (route_scheme->user) {
		id.user = route_scheme->user;
		id.user_len = strlen(route_scheme->user) + 1;
	}

	/* match or create dynamic route */
	od_route_t *route;
	route = od_routepool_match(&pooler->route_pool, &id);
	if (route)
		return route;
	route = od_routepool_new(&pooler->route_pool, route_scheme, &id);
	if (route == NULL) {
		od_error(&pooler->od->log, "failed to allocate route");
		return NULL;
	}
	return route;
}

static void
od_router_relay(void *arg)
{
	od_link_t    *link   = arg;
	od_client_t  *client = link->client;
	od_server_t *server = link->server;
	od_route_t   *route  = server->route;
	od_pooler_t  *pooler = server->pooler;

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

		if (type == 'Z') {
			od_beset_ready(server, stream);
			link->nreply++;
		}

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

static inline od_routerstatus_t
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
	relay_id = mm_create(pooler->env, od_router_relay, &link);
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
		link.nrequest++;
	}

	/* stop server relay and wait for its completion */
	if (link.server_is_active) {
		rc = mm_cancel(pooler->env, relay_id);
		assert(rc == 0);
	}
	rc = mm_wait(pooler->env, relay_id);
	assert(rc == 0);

	/* set server ready status */
	server->is_ready = (link.nrequest == link.nreply);
	return link.rc;
}

void od_router(void *arg)
{
	od_client_t *client = arg;
	od_pooler_t *pooler = client->pooler;

	od_debug(&pooler->od->log, "C: new connection");

	/* client startup */
	int rc = od_festartup(client);
	if (rc == -1) {
		od_feclose(client);
		return;
	}
	/* client cancel request */
	if (client->startup.is_cancel) {
		od_debug(&pooler->od->log, "C: cancel request");
		so_key_t key = client->startup.key;
		od_feclose(client);
		od_cancel(pooler, &key);
		return;
	}

	/* Generate backend key for the client.
	 *
	 * This key will be used to identify a server by
	 * user cancel requests. The key must be regenerated
	 * for each new client-server assignment, to avoid
	 * possibility of cancelling requests by a previous
	 * server owners.
	 */
	od_fekey(client);

	/* client auth */
	rc = od_feauth(client);
	if (rc == -1) {
		od_feclose(client);
		return;
	}
	/* notify client that we are ready */
	rc = od_feready(client);
	if (rc == -1) {
		od_feclose(client);
		return;
	}

	/* execute pooler method */
	od_routerstatus_t status;
	switch (pooler->od->scheme.pooling_mode) {
	case OD_PSESSION:
		status = od_router_session(client);
		break;
	case OD_PTRANSACTION:
	case OD_PSTATEMENT:
	case OD_PUNDEF:
		status = OD_RS_UNDEF;
		break;
	}

	od_server_t *server = client->server;
	switch (status) {
	case OD_RS_EROUTE:
	case OD_RS_EPOOL:
		assert(! client->server);
		od_feclose(client);
		break;
	case OD_RS_OK:
	case OD_RS_ECLIENT_READ:
	case OD_RS_ECLIENT_WRITE:
		/* close client connection and reuse server
		 * link in case of client errors and
		 * graceful shutdown */
		od_feclose(client);
		od_bereset(server);
		break;
	case OD_RS_ESERVER_READ:
	case OD_RS_ESERVER_WRITE:
		/* close client connection and close server
		 * connection in case of server errors */
		od_feclose(client);
		od_beclose(server);
		break;
	case OD_RS_UNDEF:
		assert(0);
		break;
	}
}
