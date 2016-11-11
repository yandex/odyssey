
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
#include "od_read.h"
#include "od_pooler.h"
#include "od_router.h"
#include "od_fe.h"
#include "od_be.h"

static odroute_t*
od_route(odpooler_t *pooler, sobestartup_t *startup)
{
	(void)pooler;
	(void)startup;
#if 0
	char *database = NULL;
	int   database_len = 0;
	char *user = NULL;
	int   user_len;

	odscheme_route_t *route_scheme;
	route_scheme = od_schemeroute_match(&pooler->od->scheme, NULL);
	if (route_scheme == NULL) {
		route_scheme = pooler->od->scheme.routing_default;
		if (route_scheme == NULL)
			return NULL;
	}

	database = NULL;  /* startup->database */
	database_len = 0; /* startup->database_len */
	user = NULL;      /* startup->user */
	user_len = 0;     /* startup->user_len */

	if (route_scheme->database) {
		database = route_scheme->database;
		database_len = strlen(database);
	}
	if (route_scheme->user) {
		user = route_scheme->user;
		user_len = strlen(user);
	}

	odroute_t *route;
	route = od_routepool_match(&pooler->route_pool,
	                           database, database_len,
	                           user, user_len);
	if (route)
		return route;
	route = od_routepool_new(&pooler->route_pool,
	                         route_scheme,
	                         database, database_len,
	                         user, user_len);
	if (route)
		return route;
	/* error */
#endif
	return NULL;
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

	/* route client */
	odroute_t *route = od_route(pooler, &client->startup);

	/* get server connection for the route */
	odserver_t *server = od_bepop(pooler, route);
	if (server == NULL) {
		od_feclose(client);
		return;
	}

	/* link server with client */
	client->server = server;

	while (1) {
		rc = od_read(client->io, &client->stream);
		if (rc == -1) {
			od_feclose(client);
			return;
		}
		char type = *client->stream.s;
		od_log(&pooler->od->log, "C: %c", type);

		/* write(server, packet) */
		while (1) {
			/* packet = read(server) */
			/* write(client, packet) */
			/* if Z break */
		}
	}
}
