
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
#include <signal.h>

#include <machinarium.h>
#include <soprano.h>

#include "od_macro.h"
#include "od_version.h"
#include "od_list.h"
#include "od_pid.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_daemon.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"
#include "od_msg.h"
#include "od_system.h"
#include "od_instance.h"

#include "od_server.h"
#include "od_server_pool.h"
#include "od_client.h"
#include "od_client_pool.h"
#include "od_route_id.h"
#include "od_route.h"
#include "od_route_pool.h"
#include "od_io.h"

#include "od_pooler.h"
#include "od_router.h"
#include "od_relay.h"
#include "od_frontend.h"
#include "od_backend.h"
#include "od_auth.h"

void od_backend_close(od_server_t *server)
{
	od_route_t *route = server->route;
	od_serverpool_set(&route->server_pool, server, OD_SUNDEF);
	if (server->io) {
		machine_close(server->io);
		machine_io_free(server->io);
		server->io = NULL;
	}
	if (server->tls) {
		machine_tls_free(server->tls);
		server->tls = NULL;
	}
	server->is_transaction = 0;
	server->idle_time = 0;
	so_keyinit(&server->key);
	so_keyinit(&server->key_client);
	od_server_free(server);
}

static inline int
od_backend_startup(od_server_t *server)
{
	od_instance_t *instance = server->system->instance;
	od_route_t *route = server->route;

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
	if (rc == -1) {
		od_error(&instance->log, server->io, "S (startup): write error: %s",
		         machine_error(server->io));
		return -1;
	}
	server->count_request++;
	return 0;
}

int od_backend_ready(od_server_t *server, uint8_t *data, int size)
{
	int status;
	int rc;
	rc = so_feread_ready(&status, data, size);
	if (rc == -1)
		return -1;
	if (status == 'I') {
		/* no active transaction */
		server->is_transaction = 0;
	} else
	if (status == 'T' || status == 'E') {
		/* in active transaction or in interrupted
		 * transaction block */
		server->is_transaction = 1;
	}
	server->count_reply++;
	return 0;
}

static inline int
od_backend_ready_wait(od_server_t *server, char *procedure, int time_ms)
{
	od_instance_t *instance = server->system->instance;

	so_stream_t *stream = &server->stream;
	/* wait for response */
	while (1) {
		so_stream_reset(stream);
		int rc;
		rc = od_read(server->io, stream, time_ms);
		if (rc == -1) {
			od_error(&instance->log, server->io, "S (%s): read error: %s",
			         procedure, machine_error(server->io));
			return -1;
		}
		uint8_t type = stream->s[rc];
		od_debug(&instance->log, server->io, "S (%s): %c",
		         procedure, type);
		/* ReadyForQuery */
		if (type == 'Z') {
			od_backend_ready(server, stream->s + rc,
			                 so_stream_used(stream) - rc);
			break;
		}
	}
	return 0;
}

static inline int
od_backend_setup(od_server_t *server)
{
	od_instance_t *instance = server->system->instance;
	so_stream_t *stream = &server->stream;
	while (1) {
		so_stream_reset(stream);
		int rc;
		rc = od_read(server->io, &server->stream, UINT32_MAX);
		if (rc == -1) {
			od_error(&instance->log, server->io, "S (setup): read error: %s",
			         machine_error(server->io));
			return -1;
		}
		uint8_t type = *server->stream.s;
		od_debug(&instance->log, server->io, "S (setup): %c", type);
		switch (type) {
		/* ReadyForQuery */
		case 'Z':
			od_backend_ready(server, stream->s, so_stream_used(stream));
			return 0;
		/* Authentication */
		case 'R':
			rc = od_auth_backend(server);
			if (rc == -1)
				return -1;
			break;
		/* BackendKeyData */
		case 'K':
			rc = so_feread_key(&server->key,
			                   stream->s, so_stream_used(stream));
			if (rc == -1) {
				od_error(&instance->log, server->io,
				         "S (setup): failed to parse BackendKeyData message");
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
			od_debug(&instance->log, server->io,
			         "S (setup): unknown packet: %c", type);
			return -1;
		}
	}
	return 0;
}

static inline int
od_backend_connect(od_server_t *server)
{
	od_instance_t *instance = server->system->instance;
	od_route_t *route = server->route;
	od_schemeserver_t *server_scheme = route->scheme->server;

	/* resolve server address */
	char port[16];
	snprintf(port, sizeof(port), "%d", server_scheme->port);
	struct addrinfo *ai = NULL;
	int rc;
	rc = machine_getaddrinfo(server_scheme->host, port, NULL, &ai, 0);
	if (rc < 0) {
		od_error(&instance->log, NULL, "failed to resolve %s:%d",
		         server_scheme->host,
		         server_scheme->port);
		return -1;
	}
	assert(ai != NULL);

	/* connect to server */
	rc = machine_connect(server->io, ai->ai_addr, UINT32_MAX);
	freeaddrinfo(ai);
	if (rc < 0) {
		od_error(&instance->log, NULL, "failed to connect to %s:%d",
		         server_scheme->host,
		         server_scheme->port);
		return -1;
	}

#if 0
	rc = machine_set_readahead(server->io, instance->scheme.readahead);
	if (rc == -1) {
		od_error(&instance->log, NULL, "failed to set readahead");
		return -1;
	}
#endif

	/* do tls handshake */
#if 0
	if (server_scheme->tls_verify != OD_TDISABLE) {
		rc = od_tlsbe_connect(pooler->env, server->io, server->tls,
		                      &server->stream,
		                      &pooler->od->log, "S",
		                      server_scheme);
		if (rc == -1)
			return -1;
	}
#endif

	od_log(&instance->log, server->io, "S: new connection");

	/* startup */
	rc = od_backend_startup(server);
	if (rc == -1)
		return -1;

	/* server configuration */
	rc = od_backend_setup(server);
	if (rc == -1)
		return -1;

	return 0;
}

od_server_t*
od_backend_new(od_router_t *router, od_route_t *route)
{
	od_instance_t *instance = router->system->instance;

	/* create new server connection */
	od_server_t *server;
	server = od_server_allocate();
	if (server == NULL)
		return NULL;
	server->route = route;
	server->system = router->system;

	/* set network options */
	server->io = machine_io_create();
	if (server->io == NULL) {
		od_server_free(server);
		return NULL;
	}
	machine_set_nodelay(server->io, instance->scheme.nodelay);
	if (instance->scheme.keepalive > 0)
		machine_set_keepalive(server->io, 1, instance->scheme.keepalive);

	/* set tls options */
#if 0
	od_schemeserver_t *server_scheme;
	server_scheme = route->scheme->server;
	if (server_scheme->tls_verify != OD_TDISABLE) {
		server->tls = od_tlsbe(pooler->env, server_scheme);
		if (server->tls == NULL) {
			od_serverfree(server);
			return NULL;
		}
	}
#endif

	int rc;
	rc = od_backend_connect(server);
	if (rc == -1) {
		od_backend_close(server);
		return NULL;
	}
	return server;
}

static inline int
od_backend_query(od_server_t *server, char *procedure, char *query, int len)
{
	od_instance_t *instance = server->system->instance;
	int rc;
	so_stream_t *stream = &server->stream;
	so_stream_reset(stream);
	rc = so_fewrite_query(stream, query, len);
	if (rc == -1)
		return -1;
	rc = od_write(server->io, stream);
	if (rc == -1) {
		od_error(&instance->log, server->io, "S (%s): write error: %s",
		         procedure, machine_error(server->io));
		return -1;
	}
	server->count_request++;
	rc = od_backend_ready_wait(server, procedure, UINT32_MAX);
	if (rc == -1)
		return -1;
	return 0;
}

int od_backend_configure(od_server_t *server, so_bestartup_t *startup)
{
	od_instance_t *instance = server->system->instance;

	char query_configure[1024];
	int  size = 0;
	so_parameter_t *param;
	so_parameter_t *end;
	param = (so_parameter_t*)startup->params.buf.s;
	end = (so_parameter_t*)startup->params.buf.p;
	for (; param < end; param = so_parameter_next(param)) {
		if (param == startup->user ||
		    param == startup->database)
			continue;
		size += snprintf(query_configure + size,
		                 sizeof(query_configure) - size,
		                 "SET %s=%s;",
		                 so_parameter_name(param),
		                 so_parameter_value(param));
	}
	if (size == 0)
		return 0;
	od_debug(&instance->log, server->io,
	         "S (configure): %s", query_configure);
	int rc;
	rc = od_backend_query(server, "configure", query_configure,
	                      size + 1);
	return rc;
}

int od_backend_reset(od_server_t *server)
{
	od_instance_t *instance = server->system->instance;
	od_route_t *route = server->route;

	/* server left in copy mode */
	if (server->is_copy) {
		od_debug(&instance->log, server->io,
		         "S (reset): in copy, closing");
		goto drop;
	}

	/* support route rollback off */
	if (! route->scheme->rollback) {
		if (server->is_transaction) {
			od_debug(&instance->log, server->io,
			         "S (reset): in active transaction, closing");
			goto drop;
		}
	}

	/* support route cancel off */
	if (! route->scheme->cancel) {
		if (! od_server_is_sync(server)) {
			od_debug(&instance->log, server->io,
			         "S (reset): not synchronized, closing");
			goto drop;
		}
	}

	/* Server is not synchronized.
	 *
	 * Number of queries sent to server is not equal
	 * to the number of received replies. Do the following
	 * logic until server becomes synchronized:
	 *
	 * 1. Wait each ReadyForQuery until we receive all
	 *    replies with 1 sec timeout.
	 *
	 * 2. Send Cancel in other connection.
	 *
	 *    It is possible that client could previously
	 *    pipeline server with requests. Each request
	 *    may stall database on its own way and may require
	 *    additional Cancel request.
	 *
	 * 3. Continue with (1)
	 */
	int wait_timeout = 1000;
	int wait_try = 0;
	int wait_try_cancel = 0;
	int wait_cancel_limit = 1;
	int rc = 0;
	for (;;) {
		while (! od_server_is_sync(server)) {
			od_debug(&instance->log, server->io,
			         "S (reset): not synchronized, wait for %d msec (#%d)",
			         wait_timeout,
			         wait_try);
			wait_try++;
			rc = od_backend_ready_wait(server, "reset", wait_timeout);
			if (rc == -1)
				break;
		}
		if (rc == -1) {
			if (! machine_read_timedout(server->io))
				goto error;
			if (wait_try_cancel == wait_cancel_limit) {
				od_debug(&instance->log, server->io,
				         "S (reset): server cancel limit reached, closing");
				goto error;
			}
			od_debug(&instance->log, server->io,
			         "S (reset): not responded, cancel (#%d)",
			         wait_try_cancel);
			wait_try_cancel++;
			/* TODO: */
			/*
			rc = od_cancel_of(pooler, route->scheme->server, &server->key);
			if (rc < 0)
				goto error;
			*/
			continue;
		}
		assert(od_server_is_sync(server));
		break;
	}
	od_debug(&instance->log, server->io, "S (reset): synchronized");

	/* send rollback in case server has an active
	 * transaction running */
	if (route->scheme->rollback) {
		if (server->is_transaction) {
			char query_rlb[] = "ROLLBACK";
			rc = od_backend_query(server, "reset rollback", query_rlb,
			                      sizeof(query_rlb));
			if (rc == -1)
				goto error;
			assert(! server->is_transaction);
		}
	}

	/* send reset query */
	if (route->scheme->discard) {
		char query_reset[] = "DISCARD ALL";
		rc = od_backend_query(server, "reset", query_reset,
		                      sizeof(query_reset));
		if (rc == -1)
			goto error;
	}

	/* ready to use */
	return  1;
drop:
	return  0;
error:
	return -1;
}
