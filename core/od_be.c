
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
#include "od_be.h"

int od_beterminate(odserver_t *server)
{
	int rc;
	sostream_t *stream = &server->stream;
	so_stream_reset(stream);
	rc = so_fewrite_terminate(stream);
	if (rc == -1)
		return -1;
	rc = od_write(server->io, stream);
	if (rc == -1)
		return -1;
	return 0;
}

int od_beclose(odserver_t *server)
{
	odroute_t *route = server->route;
	od_serverpool_set(&route->server_pool, server, OD_SUNDEF);
	if (server->io) {
		ft_close(server->io);
		server->io = NULL;
	}
	server->is_transaction = 0;
	server->is_ready = 0;
	server->idle_time = 0;
	so_keyinit(&server->key);
	so_keyinit(&server->key_client);
	od_serverfree(server);
	return 0;
}

static int
od_bestartup(odserver_t *server)
{
	odroute_t *route = server->route;
	sostream_t *stream = &server->stream;
	so_stream_reset(stream);
	sofearg_t argv[] = {
		{ "user", 5 },     { route->id.user, route->id.user_len },
		{ "database", 9 }, { route->id.database, route->id.database_len }
	};
	int rc;
	rc = so_fewrite_startup_message(stream, 4, argv);
	if (rc == -1)
		return -1;
	rc = od_write(server->io, stream);
	return rc;
}

static int
od_beauth(odserver_t *server)
{
	odpooler_t *pooler = server->pooler;
	sostream_t *stream = &server->stream;
	while (1) {
		int rc;
		rc = od_read(server->io, &server->stream);
		if (rc == -1)
			return -1;
		char type = *server->stream.s;
		od_debug(&pooler->od->log, "S: %c", type);
		switch (type) {
		/* ReadyForQuery */
		case 'Z':
			return 0;
		/* Authentication */
		case 'R':
			break;
		/* BackendKeyData */
		case 'K':
			rc = so_feread_key(&server->key,
			                   stream->s, so_stream_used(stream));
			if (rc == -1) {
				od_error(&pooler->od->log,
				         "failed to parse BackendKeyData message");
				return -1;
			}
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
			od_debug(&pooler->od->log, "unknown packet: %c", type);
			return -1;
		}
	}
	return 0;
}

static int
od_beconnect(odpooler_t *pooler, odserver_t *server)
{
	odroute_t *route = server->route;
	odscheme_server_t *server_scheme = route->scheme->server;

	/* place server to connect pool */
	od_serverpool_set(&route->server_pool, server, OD_SCONNECT);

	/* connect to server */
	int rc;
	rc = ft_connect(server->io, server_scheme->host,
	                server_scheme->port, 0);
	if (rc < 0) {
		od_error(&pooler->od->log, "failed to connect to %s:%d",
		         server_scheme->host,
		         server_scheme->port);
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
	od_serverpool_set(&route->server_pool, server, OD_SIDLE);
	return 0;
}

odserver_t*
od_bepop(odpooler_t *pooler, odroute_t *route)
{
	/* try to fetch server from idle pool */
	odserver_t *server =
		od_serverpool_pop(&route->server_pool, OD_SIDLE);
	if (server) {
		server->idle_time = 0;
		goto ready;
	}
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
		od_beclose(server);
		return NULL;
	}
ready:
	/* server is ready to use */
	od_serverpool_set(&route->server_pool, server,
	                  OD_SACTIVE);
	return server;
}

int od_beset_not_ready(odserver_t *server)
{
	server->is_ready = 0;
	return 0;
}

int od_beset_ready(odserver_t *server, sostream_t *stream)
{
	int status;
	so_feread_ready(&status, stream->s, so_stream_used(stream));
	if (status == 'I') {
		/* no active transaction */
		server->is_transaction = 0;
	} else
	if (status == 'T' || status == 'E') {
		/* in active transaction or in interrupted
		 * transaction block */
		server->is_transaction = 1;
	}
	server->is_ready = 1;
	return 0;
}

static inline int
od_beready_wait(odserver_t *server, char *procedure)
{
	odpooler_t *pooler = server->pooler;
	sostream_t *stream = &server->stream;

	so_stream_reset(stream);
	/* wait for responce */
	while (1) {
		int rc;
		rc = od_read(server->io, stream);
		if (rc == -1)
			return -1;
		uint8_t type = *stream->s;
		od_debug(&pooler->od->log, "S (%s): %c", procedure, type);
		/* ReadyForQuery */
		if (type == 'Z')
			break;
		/* ErrorResponce */
		if (type == 'E')
			return -2;
	}
	return 0;
}

static inline int
od_bequery(odserver_t *server, char *procedure, char *query, int len)
{
	int rc;
	sostream_t *stream = &server->stream;
	so_stream_reset(stream);
	rc = so_fewrite_query(stream, query, len);
	if (rc == -1)
		return -1;
	rc = od_write(server->io, stream);
	if (rc == -1)
		return -1;
	rc = od_beready_wait(server, procedure);
	if (rc < 0)
		return -1;
	return 0;
}

int od_bereset(odserver_t *server)
{
	odroute_t *route = server->route;

	/* place server to reset pool */
	od_serverpool_set(&route->server_pool, server,
	                  OD_SRESET);

	if (! server->is_ready) {
		/* connect to server */
		/* send cancel and disconnect */
		/* wait for ready */
	}

	/* send rollback in case if server has an active
	 * transaction running */
	if (server->is_transaction) {
		char rollback_query[] = "ROLLBACK";
		int rc;
		rc = od_bequery(server, "rollback", rollback_query,
		                sizeof(rollback_query));
		if (rc == -1)
			goto error;
		server->is_transaction = 0;
	}

	/* send reset query */
	char reset_query[] = "DISCARD ALL";
	int rc;
	rc = od_bequery(server, "reset", reset_query,
	                sizeof(reset_query));
	if (rc == -1)
		goto error;

	/* server is ready to use */
	od_serverpool_set(&route->server_pool, server,
	                  OD_SIDLE);
	return 0;
error:
	od_beclose(server);
	return -1;
}
