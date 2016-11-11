
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
#include "od_be.h"

static int
od_beclose(odpooler_t *pooler, odserver_t *server)
{
	od_serverpool_set(&pooler->server_pool, server, OD_SUNDEF);
	if (server->io) {
		ft_close(server->io);
		server->io = NULL;
	}
	od_serverfree(server);
	return 0;
}

static int
od_bestartup(odserver_t *server)
{
	odscheme_server_t *dest = server->route->server;
	(void)dest;

	sostream_t *stream = &server->stream;
	so_stream_reset(stream);
	sofearg_t argv[] = {
		{ "user", 5 },     { "test", 5 },
		{ "database", 9 }, { "test", 5 }
	};
	int rc;
	rc = so_fewrite_startup_message(stream, 4, argv);
	if (rc == -1)
		return -1;
	rc = ft_write(server->io, (char*)stream->s,
	              so_stream_used(stream), 0);
	return rc;
}

static int
od_beauth(odserver_t *server)
{
	odpooler_t *pooler = server->pooler;
	while (1) {
		int rc;
		rc = od_read(server->io, &server->stream);
		if (rc == -1)
			return -1;
		char type = *server->stream.s;
		od_log(&pooler->od->log, "S: %c", type);
		switch (type) {
		/* ReadyForQuery */
		case 'Z':
			return 0;
		/* Authentication */
		case 'R':
			break;
		/* BackendKeyData */
		case 'K':
			break;
		/* ParameterStatus */
		case 'S':
			break;
		/* NoticeResponce */
		case 'N':
			break;
		/* ErrorResponce */
		case 'E':
			return -1;
		default:
			printf("unknown packet: %c\n",type);
			return -1;
		}
	}
	return 0;
}

static int
od_beconnect(odpooler_t *pooler, odserver_t *server)
{
	odscheme_server_t *dest = server->route->server;

	/* place server to connect pool */
	od_serverpool_set(&pooler->server_pool, server, OD_SCONNECT);

	/* connect to server */
	int rc;
	rc = ft_connect(server->io, dest->host, dest->port, 0);
	if (rc < 0) {
		od_log(&pooler->od->log, "failed to connect to %s:%d",
		       dest->host, dest->port);
		return -1;
	}
	/* startup */
	rc = od_bestartup(server);
	if (rc == -1)
		return -1;
	/* auth */
	rc = od_beauth(server);
	if (rc == -1)
		return -1;

	/* server is ready to use */
	od_serverpool_set(&pooler->server_pool, server,
	                  OD_SIDLE);
	return 0;
}

odserver_t*
od_bepop(odpooler_t *pooler, odscheme_route_t *route)
{
	/* try to fetch server from idle pool */
	odserver_t *server =
		od_serverpool_pop(&pooler->server_pool);
	if (server)
		goto ready;
	/* create new server connection */
	server = od_serveralloc();
	if (server == NULL)
		return NULL;
	server->io = ft_io_new(pooler->env);
	if (server->io == NULL) {
		od_serverfree(server);
		return NULL;
	}
	server->pooler = pooler;
	server->route = route;
	int rc;
	rc = od_beconnect(pooler, server);
	if (rc == -1) {
		od_beclose(pooler, server);
		return NULL;
	}
ready:
	/* server is ready to use */
	od_serverpool_set(&pooler->server_pool, server,
	                  OD_SACTIVE);
	return server;
}
