
/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <flint.h>
#include <soprano.h>

#include "od_macro.h"
#include "od_list.h"
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
#include "od_fe.h"
#include "od_be.h"

static odroute_t*
od_route(odpooler_t *pooler, sobestartup_t *startup)
{
	assert(startup->database != NULL);
	assert(startup->user != NULL);

	/* match required route according to scheme */
	odscheme_route_t *route_scheme;
	route_scheme =
		od_schemeroute_match(&pooler->od->scheme, startup->database);
	if (route_scheme == NULL) {
		/* try to use default route */
		route_scheme = pooler->od->scheme.routing_default;
		if (route_scheme == NULL)
			return NULL;
	}
	odroute_id_t id = {
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
	odroute_t *route;
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

static inline int
od_router_session(odclient_t *client)
{
	odpooler_t *pooler = client->pooler;

	/* client routing */
	odroute_t *route = od_route(pooler, &client->startup);
	if (route == NULL) {
		od_error(&pooler->od->log, "C: database route '%s' is not declared",
		         client->startup.database);
		return -1;
	}
	/* get server connection for the route */
	odserver_t *server = od_bepop(pooler, route);
	if (server == NULL)
		return -1;
	client->server = server;

	od_log(&pooler->od->log, "C: route to %s server",
	       route->scheme->server->name);

	sostream_t *stream = &client->stream;
	while (1) {
		/* client to server */
		int rc;
		rc = od_read(client->io, stream);
		if (rc == -1)
			return -1;
		char type = *client->stream.s;
		od_log(&pooler->od->log, "C: %c", type);

		if (type == 'X') {
			/* client graceful shutdown */
			break;
		}
		rc = od_write(server->io, stream);
		if (rc == -1) {
		}

		/* server to client */
		while (1) {
			rc = od_read(server->io, stream);
			if (rc == -1) {
			}

			type = *stream->s;
			od_log(&pooler->od->log, "S: %c", type);

			rc = od_write(client->io, stream);
			if (rc == -1) {
			}

			if (type == 'Z')
				break;
		}
	}
	return 0;
}

void od_router(void *arg)
{
	odclient_t *client = arg;
	odpooler_t *pooler = client->pooler;

	od_log(&pooler->od->log, "C: new connection");

	/* client startup */
	int rc = od_festartup(client);
	if (rc == -1) {
		od_feclose(client);
		return;
	}
	/* client cancel request */
	if (client->startup.is_cancel) {
		od_log(&pooler->od->log, "C: cancel request");
		od_feclose(client);
		return;
	}
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
	/* execute pooling method */
	switch (pooler->od->scheme.pooling_mode) {
	case OD_PSESSION:
		rc = od_router_session(client);
		break;
	case OD_PTRANSACTION:
	case OD_PSTATEMENT:
	case OD_PUNDEF:
		rc = -1;
		assert(0);
		break;
	}
	odserver_t *server = client->server;
	od_feclose(client);

	(void)server;
}
