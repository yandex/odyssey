
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

typedef enum {
	OD_RRC_CLIENT_EXIT,
	OD_RRC_CLIENT_EREAD,
	OD_RRC_CLIENT_EWRITE,
	OD_RRC_SERVER_EROUTE,
	OD_RRC_SERVER_EPOP,
	OD_RRC_SERVER_EREAD,
	OD_RRC_SERVER_EWRITE
} odrouter_rc_t;

static inline odrouter_rc_t
od_router_session(odclient_t *client)
{
	odpooler_t *pooler = client->pooler;

	/* client routing */
	odroute_t *route = od_route(pooler, &client->startup);
	if (route == NULL) {
		od_error(&pooler->od->log, "C: database route '%s' is not declared",
		         client->startup.database);
		return OD_RRC_SERVER_EROUTE;
	}
	/* get server connection for the route */
	odserver_t *server = od_bepop(pooler, route);
	if (server == NULL)
		return OD_RRC_SERVER_EPOP;
	client->server = server;

	od_log(&pooler->od->log, "C: route to %s server",
	       route->scheme->server->name);

	sostream_t *stream = &client->stream;
	int type;
	int rc;
	for (;;)
	{
		/* client to server */
		rc = od_read(client->io, stream);
		if (rc == -1)
			return OD_RRC_CLIENT_EREAD;

		type = *stream->s;
		od_log(&pooler->od->log, "C: %c", *stream->s);

		/* client graceful shutdown */
		if (type == 'X')
			return OD_RRC_CLIENT_EXIT;

		rc = od_write(server->io, stream);
		if (rc == -1)
			return OD_RRC_SERVER_EWRITE;

		/* server to client */
		while (1) {
			rc = od_read(server->io, stream);
			if (rc == -1)
				return OD_RRC_SERVER_EREAD;

			type = *stream->s;
			od_log(&pooler->od->log, "S: %c", type);

			rc = od_write(client->io, stream);
			if (rc == -1)
				return OD_RRC_CLIENT_EWRITE;

			if (type == 'Z')
				break;
		}
	}

	/* unreach */
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
	odrouter_rc_t rrc;
	switch (pooler->od->scheme.pooling_mode) {
	case OD_PSESSION:
		rrc = od_router_session(client);
		break;
	case OD_PTRANSACTION:
	case OD_PSTATEMENT:
	case OD_PUNDEF:
		assert(0);
		break;
	}

	odserver_t *server = client->server;
	(void)server;

	switch (rrc) {
	case OD_RRC_CLIENT_EXIT:
		break;
	case OD_RRC_CLIENT_EREAD:
	case OD_RRC_CLIENT_EWRITE:
		break;
	case OD_RRC_SERVER_EROUTE:
	case OD_RRC_SERVER_EPOP:
		break;
	case OD_RRC_SERVER_EREAD:
	case OD_RRC_SERVER_EWRITE:
		break;
	}
}
