
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
#include "od_cancel.h"
#include "od_be.h"

int od_beterminate(od_server_t *server)
{
	int rc;
	so_stream_t *stream = &server->stream;
	so_stream_reset(stream);
	rc = so_fewrite_terminate(stream);
	if (rc == -1)
		return -1;
	rc = od_write(server->io, stream);
	if (rc == -1)
		return -1;
	return 0;
}

int od_beclose(od_server_t *server)
{
	odroute_t *route = server->route;
	od_serverpool_set(&route->server_pool, server, OD_SUNDEF);
	if (server->io) {
		mm_close(server->io);
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
od_bestartup(od_server_t *server)
{
	odroute_t *route = server->route;
	so_stream_t *stream = &server->stream;
	so_stream_reset(stream);
	so_fearg_t argv[] = {
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
od_beauth(od_server_t *server)
{
	odpooler_t *pooler = server->pooler;
	so_stream_t *stream = &server->stream;
	while (1) {
		int rc;
		rc = od_read(server->io, &server->stream, 0);
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
od_beconnect(odpooler_t *pooler, od_server_t *server)
{
	odroute_t *route = server->route;
	od_schemeserver_t *server_scheme = route->scheme->server;

	/* place server to connect pool */
	od_serverpool_set(&route->server_pool, server, OD_SCONNECT);

	/* connect to server */
	int rc;
	rc = mm_connect(server->io, server_scheme->host,
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

od_server_t*
od_bepop(odpooler_t *pooler, odroute_t *route)
{
	/* try to fetch server from idle pool */
	od_server_t *server =
		od_serverpool_pop(&route->server_pool, OD_SIDLE);
	if (server) {
		server->idle_time = 0;
		goto ready;
	}
	/* create new server connection */
	server = od_serveralloc();
	if (server == NULL)
		return NULL;
	server->io = mm_io_new(pooler->env);
	if (server->io == NULL) {
		od_serverfree(server);
		return NULL;
	}
	mm_io_nodelay(server->io, pooler->od->scheme.nodelay);
	if (pooler->od->scheme.keepalive > 0)
		mm_io_keepalive(server->io, 1, pooler->od->scheme.keepalive);
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

int od_beset_ready(od_server_t *server, so_stream_t *stream)
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
od_beready_wait(od_server_t *server, char *procedure, int time_ms)
{
	odpooler_t *pooler = server->pooler;
	so_stream_t *stream = &server->stream;
	so_stream_reset(stream);
	/* wait for responce */
	while (1) {
		int rc;
		rc = od_read(server->io, stream, time_ms);
		if (rc == -1)
			return -1;
		uint8_t type = *stream->s;
		od_debug(&pooler->od->log, "S (%s): %c", procedure, type);
		/* ReadyForQuery */
		if (type == 'Z')
			break;
	}
	od_beset_ready(server, stream);
	return 0;
}

static inline int
od_bequery(od_server_t *server, char *procedure, char *query, int len)
{
	int rc;
	so_stream_t *stream = &server->stream;
	so_stream_reset(stream);
	rc = so_fewrite_query(stream, query, len);
	if (rc == -1)
		return -1;
	rc = od_write(server->io, stream);
	if (rc == -1)
		return -1;
	rc = od_beready_wait(server, procedure, 0);
	if (rc == -1)
		return -1;
	return 0;
}

int od_bereset(od_server_t *server)
{
	odpooler_t *pooler = server->pooler;
	odroute_t *route = server->route;

	/* place server to reset pool */
	od_serverpool_set(&route->server_pool, server,
	                  OD_SRESET);

	int rc;
	/* server is not ready.
	 *
	 * 1. Try to wait for ready status (5 sec timeout)
	 * 2. If not reponded, send Cancel in other connection
	 * 3. Wait for ready status (5 sec)
	 * 4. If not reponded, send Terminate and close connection
	 */
	if (! server->is_ready) {
		od_debug(&pooler->od->log, "S (not ready): wait for 5 seconds");
		rc = od_beready_wait(server, "not ready", 5 * 1000);
		if (rc == -1) {
			if (! mm_read_is_timeout(server->io))
				goto error;
			od_debug(&pooler->od->log, "S (not ready): not responded, initiate cancel");
			rc = od_cancel_of(pooler, route->scheme->server, &server->key);
			if (rc < 0)
					goto error;
			od_debug(&pooler->od->log, "S (not ready): wait for 5 seconds");
			rc = od_beready_wait(server, "not ready", 5 * 1000);
			if (rc == -1) {
				od_debug(&pooler->od->log, "S (not ready): not responded, dropping");
				goto error;
			}
			od_debug(&pooler->od->log, "S (not ready): ready");
		}
	}

	/* send rollback in case if server has an active
	 * transaction running */
	if (server->is_transaction) {
		char query_rlb[] = "ROLLBACK";
		rc = od_bequery(server, "rollback", query_rlb,
		                sizeof(query_rlb));
		if (rc == -1)
			goto error;
		assert(! server->is_transaction);
	}

	/* send reset query */
	char query_reset[] = "DISCARD ALL";
	rc = od_bequery(server, "reset", query_reset,
	                sizeof(query_reset));
	if (rc == -1)
		goto error;

	/* server is ready to use */
	od_serverpool_set(&route->server_pool, server,
	                  OD_SIDLE);
	return 0;
error:
	od_beterminate(server);
	od_beclose(server);
	return -1;
}
