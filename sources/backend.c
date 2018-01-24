
/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <signal.h>

#include <machinarium.h>
#include <shapito.h>

#include "sources/macro.h"
#include "sources/version.h"
#include "sources/atomic.h"
#include "sources/util.h"
#include "sources/error.h"
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/logger.h"
#include "sources/daemon.h"
#include "sources/scheme.h"
#include "sources/scheme_mgr.h"
#include "sources/config.h"
#include "sources/msg.h"
#include "sources/system.h"
#include "sources/server.h"
#include "sources/server_pool.h"
#include "sources/client.h"
#include "sources/client_pool.h"
#include "sources/route_id.h"
#include "sources/route.h"
#include "sources/route_pool.h"
#include "sources/io.h"
#include "sources/instance.h"
#include "sources/router.h"
#include "sources/pooler.h"
#include "sources/relay.h"
#include "sources/frontend.h"
#include "sources/backend.h"
#include "sources/auth.h"
#include "sources/tls.h"
#include "sources/cancel.h"

void od_backend_close(od_server_t *server)
{
	assert(server->route == NULL);
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
	shapito_key_init(&server->key);
	shapito_key_init(&server->key_client);
	od_server_free(server);
}

int od_backend_terminate(od_server_t *server)
{
	int rc;
	shapito_stream_t *stream = &server->stream;
	shapito_stream_reset(stream);
	rc = shapito_fe_write_terminate(stream);
	if (rc == -1)
		return -1;
	rc = od_write(server->io, stream);
	if (rc == -1)
		return -1;
	return 0;
}

void od_backend_error(od_server_t *server, char *context, char *data, int size)
{
	od_instance_t *instance = server->system->instance;
	shapito_fe_error_t error;
	int rc;
	rc = shapito_fe_read_error(&error, data, size);
	if (rc == -1) {
		od_error(&instance->logger, context, server->client, server,
		         "failed to parse error message from server");
		return;
	}
	od_error(&instance->logger, context, server->client, server,
	         "%s %s %s",
	         error.severity,
	         error.code,
	         error.message);
	if (error.detail) {
		od_error(&instance->logger, context, server->client, server,
		         "DETAIL: %s", error.detail);
	}
	if (error.hint) {
		od_error(&instance->logger, context, server->client, server,
		         "HINT: %s", error.hint);
	}
	od_server_stat_error(server);
}

int od_backend_ready(od_server_t *server, char *context,
                     char *data, int size)
{
	od_instance_t *instance = server->system->instance;
	int status;
	int rc;
	rc = shapito_fe_read_ready(&status, data, size);
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
	/* update reply counter and query time */
	uint64_t query_time;
	query_time = od_server_stat_reply(server);

	od_debug(&instance->logger, context, server->client, server,
	         "query time: %d microseconds",
	         query_time);
	return 0;
}

static inline int
od_backend_startup(od_server_t *server)
{
	od_instance_t *instance = server->system->instance;
	od_route_t *route = server->route;

	shapito_stream_t *stream = &server->stream;
	shapito_stream_reset(stream);
	shapito_fe_arg_t argv[] = {
		{ "user", 5 },     { route->id.user, route->id.user_len },
		{ "database", 9 }, { route->id.database, route->id.database_len }
	};
	int rc;
	rc = shapito_fe_write_startup_message(stream, 4, argv);
	if (rc == -1)
		return -1;
	rc = od_write(server->io, stream);
	if (rc == -1) {
		od_error(&instance->logger, "startup", NULL, server,
		         "write error: %s",
		         machine_error(server->io));
		return -1;
	}

	/* update request count and sync state */
	od_server_stat_request(server, 1);

	while (1) {
		shapito_stream_reset(stream);
		int rc;
		rc = od_read(server->io, &server->stream, UINT32_MAX);
		if (rc == -1) {
			od_error(&instance->logger, "startup", NULL, server,
			         "read error: %s",
			         machine_error(server->io));
			return -1;
		}
		char type = *server->stream.start;
		od_debug(&instance->logger, "startup", NULL, server,
		         "%c", type);

		switch (type) {
		/* ReadyForQuery */
		case 'Z':
			od_backend_ready(server, "startup", stream->start,
			                 shapito_stream_used(stream));
			return 0;
		/* Authentication */
		case 'R':
			rc = od_auth_backend(server);
			if (rc == -1)
				return -1;
			break;
		/* BackendKeyData */
		case 'K':
			rc = shapito_fe_read_key(&server->key, stream->start,
			                         shapito_stream_used(stream));
			if (rc == -1) {
				od_error(&instance->logger, "startup", NULL, server,
				         "failed to parse BackendKeyData message");
				return -1;
			}
			break;
		/* ParameterStatus */
		case 'S':
		{
			char *name;
			uint32_t name_len;
			char *value;
			uint32_t value_len;
			rc = shapito_fe_read_parameter(stream->start,
			                               shapito_stream_used(stream),
			                               &name, &name_len,
			                               &value, &value_len);
			if (rc == -1) {
				od_error(&instance->logger, "startup", NULL, server,
				         "failed to parse ParameterStatus message");
				return -1;
			}
			rc = shapito_parameters_add(&server->params, name, name_len,
			                            value, value_len);
			if (rc == -1)
				return -1;
			break;
		}
		/* NoticeResponse */
		case 'N':
			break;
		/* ErrorResponse */
		case 'E':
			od_backend_error(server, "startup", stream->start,
			                 shapito_stream_used(stream));
			return -1;
		default:
			od_debug(&instance->logger, "startup", NULL, server,
			         "unknown packet: %c", type);
			return -1;
		}
	}
	return 0;
}

static int
od_backend_connect_to(od_server_t *server,
                      od_schemestorage_t *server_scheme,
                      char *context)
{
	od_instance_t *instance = server->system->instance;
	assert(server->io == NULL);

	/* create io handle */
	server->io = machine_io_create();
	if (server->io == NULL)
		return -1;

	/* set network options */
	machine_set_nodelay(server->io, instance->scheme.nodelay);
	if (instance->scheme.keepalive > 0)
		machine_set_keepalive(server->io, 1, instance->scheme.keepalive);
	int rc;
	rc = machine_set_readahead(server->io, instance->scheme.readahead);
	if (rc == -1) {
		od_error(&instance->logger, context, NULL, server,
		         "failed to set readahead");
		return -1;
	}

	/* set tls options */
	if (server_scheme->tls_mode != OD_TLS_DISABLE) {
		server->tls = od_tls_backend(server_scheme);
		if (server->tls == NULL)
			return -1;
	}

	/* resolve server address */
	char port[16];
	od_snprintf(port, sizeof(port), "%d", server_scheme->port);
	struct addrinfo *ai = NULL;
	rc = machine_getaddrinfo(server_scheme->host, port, NULL, &ai, 0);
	if (rc != 0) {
		od_error(&instance->logger, context, NULL, server,
		         "failed to resolve %s:%d",
		         server_scheme->host,
		         server_scheme->port);
		return -1;
	}
	assert(ai != NULL);

	/* connect to server */
	rc = machine_connect(server->io, ai->ai_addr, UINT32_MAX);
	freeaddrinfo(ai);
	if (rc == -1) {
		od_error(&instance->logger, context, server->client, server,
		         "failed to connect to %s:%d",
		         server_scheme->host,
		         server_scheme->port);
		return -1;
	}

	/* do tls handshake */
	if (server_scheme->tls_mode != OD_TLS_DISABLE) {
		rc = od_tls_backend_connect(server, &instance->logger, server_scheme);
		if (rc == -1)
			return -1;
	}

	return 0;
}

int od_backend_connect(od_server_t *server, char *context)
{
	od_instance_t *instance = server->system->instance;
	od_route_t *route = server->route;
	assert(route != NULL);

	od_schemestorage_t *server_scheme;
	server_scheme = route->scheme->storage;

	/* connect to server */
	int rc;
	rc = od_backend_connect_to(server, server_scheme, context);
	if (rc == -1)
		return -1;

	/* log server connection */
	if (instance->scheme.log_session) {
		od_log(&instance->logger, context, server->client, server,
		       "new server connection %s:%d",
		       server_scheme->host,
		       server_scheme->port);
	}

	/* send startup and do initial configuration */
	rc = od_backend_startup(server);
	if (rc == -1)
		return -1;

	return 0;
}

int od_backend_connect_cancel(od_server_t *server,
                              od_schemestorage_t *server_scheme,
                              shapito_key_t *key)
{
	od_instance_t *instance = server->system->instance;
	/* connect to server */
	int rc;
	rc = od_backend_connect_to(server, server_scheme, "cancel");
	if (rc == -1)
		return -1;
	/* send cancel request */
	shapito_stream_reset(&server->stream);
	rc = shapito_fe_write_cancel(&server->stream, key->key_pid, key->key);
	if (rc == -1)
		return -1;
	rc = od_write(server->io, &server->stream);
	if (rc == -1) {
		od_error(&instance->logger, "cancel", NULL, NULL,
		         "write error: %s",
		         machine_error(server->io));
	}
	return 0;
}

int od_backend_ready_wait(od_server_t *server, char *context, int count, int time_ms)
{
	od_instance_t *instance = server->system->instance;

	shapito_stream_t *stream = &server->stream;
	/* wait for response */

	int ready = 0;
	for (;;)
	{
		shapito_stream_reset(stream);
		int rc;
		rc = od_read(server->io, stream, time_ms);
		if (rc == -1) {
			if (! machine_timedout()) {
				od_error(&instance->logger, context, server->client, server,
				         "read error: %s",
				         machine_error(server->io));
			}
			return -1;
		}
		int offset = rc;
		char type = stream->start[offset];
		od_debug(&instance->logger, context, server->client, server,
		         "%c", type);
		/* ErrorResponse */
		if (type == 'E') {
			od_backend_error(server, context, stream->start,
			                 shapito_stream_used(stream));
			continue;
		}
		/* ParameterStatus */
		if (type == 'S') {
			char *name;
			uint32_t name_len;
			char *value;
			uint32_t value_len;
			rc = shapito_fe_read_parameter(stream->start,
			                               shapito_stream_used(stream),
			                               &name, &name_len,
			                               &value, &value_len);
			if (rc == -1) {
				od_error(&instance->logger, context, server->client, server,
				         "failed to parse ParameterStatus message");
				return -1;
			}
			/* update parameter */
			rc = shapito_parameters_update(&server->params, name, name_len,
			                               value, value_len);
			if (rc == -1)
				return -1;
			od_debug(&instance->logger, context, server->client, server,
			         "%.*s = %.*s",
			         name_len, name, value_len, value);
			continue;
		}
		/* ReadyForQuery */
		if (type == 'Z') {
			od_backend_ready(server, context,
			                 stream->start + offset,
			                 shapito_stream_used(stream) - offset);
			ready++;
			if (ready == count)
				break;
		}
	}
	return 0;
}

int od_backend_query(od_server_t *server, char *context,
                     char *query, int len)
{
	od_instance_t *instance = server->system->instance;
	int rc;
	shapito_stream_t *stream = &server->stream;
	shapito_stream_reset(stream);
	rc = shapito_fe_write_query(stream, query, len);
	if (rc == -1)
		return -1;
	rc = od_write(server->io, stream);
	if (rc == -1) {
		od_error(&instance->logger, context, server->client, server,
		         "write error: %s",
		         machine_error(server->io));
		return -1;
	}

	/* update server sync state and stats */
	od_server_stat_request(server, 1);

	rc = od_backend_ready_wait(server, context, 1, UINT32_MAX);
	if (rc == -1)
		return -1;
	return 0;
}
