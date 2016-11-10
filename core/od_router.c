
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
#include "od_client.h"
#include "od_client_pool.h"
#include "od.h"
#include "od_read.h"
#include "od_pooler.h"
#include "od_router.h"
#include "od_fe.h"
#include "od_be.h"

static odserver_t*
od_beconnect(odpooler_t *pooler, sobestartup_t *startup)
{
	(void)pooler;
	(void)startup;
	return NULL;
}

static odserver_t*
od_bepop(odpooler_t *pooler, sobestartup_t *startup)
{
	odserver_t *server = od_serverpool_pop(&pooler->server_pool);
	if (server) {
		od_serverpool_set(&pooler->server_pool, server, OD_SACTIVE);
		return server;
	}
	server = od_beconnect(pooler, startup);
	return server;
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

	/* get server connection */
	odserver_t *server = od_bepop(pooler, &client->startup);
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
