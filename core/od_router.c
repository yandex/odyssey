
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
#include "od_router_transaction.h"
#include "od_cancel.h"
#include "od_auth.h"
#include "od_fe.h"
#include "od_be.h"

od_route_t*
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
		od_error(&pooler->od->log, NULL,
		         "failed to allocate route");
		return NULL;
	}
	return route;
}

void od_router(void *arg)
{
	od_client_t *client = arg;
	od_pooler_t *pooler = client->pooler;

	od_log(&pooler->od->log, client->io, "C: new connection");

	/* client startup */
	int rc = od_festartup(client);
	if (rc == -1) {
		od_feclose(client);
		return;
	}
	/* client cancel request */
	if (client->startup.is_cancel) {
		od_debug(&pooler->od->log, client->io, "C: cancel request");
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

	/* client authentication */
	rc = od_authfe(client);
	if (rc == -1) {
		od_feclose(client);
		return;
	}

	/* set client backend options and the key */
	rc = od_fesetup(client);
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
	od_routerstatus_t status = OD_RS_UNDEF;
	switch (pooler->od->scheme.pooling_mode) {
	case OD_PSESSION:
		status = od_router_session(client);
		break;
	case OD_PTRANSACTION:
		status = od_router_transaction(client);
		break;
	case OD_PUNDEF:
		break;
	}

	od_server_t *server = client->server;
	switch (status) {
	case OD_RS_EROUTE:
	case OD_RS_EPOOL:
	case OD_RS_ELIMIT:
		assert(! client->server);
		od_feclose(client);
		break;
	case OD_RS_OK:
	case OD_RS_ECLIENT_READ:
	case OD_RS_ECLIENT_WRITE:
		if (status == OD_RS_OK)
			od_log(&pooler->od->log, client->io,
			       "C: disconnected");
		else
			od_log(&pooler->od->log, client->io,
			       "C: disconnected (read/write error)");
		/* close client connection and reuse server
		 * link in case of client errors and
		 * graceful shutdown */
		od_feclose(client);
		if (server)
			od_bereset(server);
		break;
	case OD_RS_ESERVER_READ:
	case OD_RS_ESERVER_WRITE:
		od_log(&pooler->od->log, server->io,
		       "S: disconnected (read/write error)");
		/* close client connection and close server
		 * connection in case of server errors */
		od_feclose(client);
		if (server)
			od_beclose(server);
		break;
	case OD_RS_UNDEF:
		assert(0);
		break;
	}
}
