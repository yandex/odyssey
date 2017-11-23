
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
#include "sources/error.h"
#include "sources/atomic.h"
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
#include "sources/reset.h"
#include "sources/tls.h"
#include "sources/auth.h"
#include "sources/console.h"

typedef enum {
	OD_FE_UNDEF,
	OD_FE_OK,
	OD_FE_EATTACH,
	OD_FE_ESERVER_CONNECT,
	OD_FE_ESERVER_CONFIGURE,
	OD_FE_ESERVER_READ,
	OD_FE_ESERVER_WRITE,
	OD_FE_ECLIENT_READ,
	OD_FE_ECLIENT_WRITE,
	OD_FE_ECLIENT_CONFIGURE
} od_frontend_rc_t;

void od_frontend_close(od_client_t *client)
{
	assert(client->route == NULL);
	assert(client->server == NULL);
	if (client->io) {
		machine_close(client->io);
		machine_io_free(client->io);
		client->io = NULL;
	}
	od_client_free(client);
}

static inline int
od_frontend_error_fwd(od_client_t *client)
{
	od_server_t *server;
	server = client->server;
	assert(server != NULL);
	assert(server->stats.count_error != 0);
	shapito_fe_error_t error;
	int rc;
	rc = shapito_fe_read_error(&error, server->stream.start,
	                           shapito_stream_used(&server->stream));
	if (rc == -1)
		return -1;
	shapito_stream_t *stream = &client->stream;
	shapito_stream_reset(stream);
	char msg[512];
	int  msg_len;
	msg_len = snprintf(msg, sizeof(msg), "odissey: %s%.*s: %s",
	                   client->id.id_prefix,
	                   (signed)sizeof(client->id.id),
	                   client->id.id,
	                   error.message);
	int detail_len = error.detail ? strlen(error.detail) : 0;
	int hint_len   = error.hint ? strlen(error.hint) : 0;
	rc = shapito_be_write_error_as(stream,
	                               error.severity,
	                               error.code,
	                               error.detail, detail_len,
	                               error.hint, hint_len,
	                               msg, msg_len);
	if (rc == -1)
		return -1;
	rc = od_write(client->io, stream);
	return rc;
}

static inline int
od_frontend_verror(od_client_t *client, char *code, char *fmt, va_list args)
{
	char msg[512];
	int  msg_len;
	msg_len = snprintf(msg, sizeof(msg), "odissey: %s%.*s: ",
	                   client->id.id_prefix,
	                   (signed)sizeof(client->id.id),
	                   client->id.id);
	msg_len += vsnprintf(msg + msg_len, sizeof(msg) - msg_len, fmt, args);
	shapito_stream_t *stream = &client->stream;
	int rc;
	rc = shapito_be_write_error(stream, code, msg, msg_len);
	if (rc == -1)
		return -1;
	return 0;
}

int od_frontend_errorf(od_client_t *client, char *code, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int rc;
	rc = od_frontend_verror(client, code, fmt, args);
	va_end(args);
	return rc;
}

int od_frontend_error(od_client_t *client, char *code, char *fmt, ...)
{
	shapito_stream_t *stream = &client->stream;
	shapito_stream_reset(stream);
	va_list args;
	va_start(args, fmt);
	int rc;
	rc = od_frontend_verror(client, code, fmt, args);
	va_end(args);
	if (rc == -1)
		return -1;
	rc = od_write(client->io, stream);
	return rc;
}

static int
od_frontend_startup_read(od_client_t *client)
{
	od_instance_t *instance = client->system->instance;

	shapito_stream_t *stream = &client->stream;
	shapito_stream_reset(stream);
	for (;;) {
		uint32_t pos_size = shapito_stream_used(stream);
		char *pos_data = stream->start;
		uint32_t len;
		int to_read;
		to_read = shapito_read_startup(&len, &pos_data, &pos_size);
		if (to_read == 0)
			break;
		if (to_read == -1) {
			od_error(&instance->logger, "startup", client, NULL,
			         "failed to read startup packet, closing");
			return -1;
		}
		int rc = shapito_stream_ensure(stream, to_read);
		if (rc == -1)
			return -1;
		rc = machine_read(client->io, stream->pos, to_read, UINT32_MAX);
		if (rc == -1) {
			od_error(&instance->logger, "startup", client, NULL,
			         "read error: %s",
			         machine_error(client->io));
			return -1;
		}
		shapito_stream_advance(stream, to_read);
	}
	return 0;
}

static int
od_frontend_startup(od_client_t *client)
{
	od_instance_t *instance = client->system->instance;

	int rc;
	rc = od_frontend_startup_read(client);
	if (rc == -1)
		return -1;
	shapito_stream_t *stream = &client->stream;
	rc = shapito_be_read_startup(&client->startup, stream->start,
	                             shapito_stream_used(stream));
	if (rc == -1)
		goto error;

	/* client ssl request */
	rc = od_tls_frontend_accept(client, &instance->logger,
	                            client->scheme_listen,
	                            client->tls);
	if (rc == -1)
		return -1;

	if (! client->startup.is_ssl_request)
		return 0;

	/* read startup-cancel message followed after ssl
	 * negotiation */
	assert(client->startup.is_ssl_request);
	rc = od_frontend_startup_read(client);
	if (rc == -1)
		return -1;
	rc = shapito_be_read_startup(&client->startup, stream->start,
	                             shapito_stream_used(stream));
	if (rc == -1)
		goto error;
	return 0;

error:
	od_error(&instance->logger, "startup", client, NULL,
	         "incorrect startup packet");
	od_frontend_error(client, SHAPITO_PROTOCOL_VIOLATION,
	                  "bad startup packet");
	return -1;
}

static inline void
od_frontend_key(od_client_t *client)
{
	/* Generate backend key for the client.
	 *
	 * This key will be used to identify a server by
	 * user cancel requests. The key must be regenerated
	 * for each new client-server assignment, to avoid
	 * possibility of cancelling requests by a previous
	 * server owners.
	 */
	client->key.key_pid = *(int32_t*)client->id.id;
	client->key.key = 1 + rand();
}

static inline int
od_frontend_setup_console(shapito_stream_t *stream)
{
	int rc;
	/* console parameters */
	rc = shapito_be_write_parameter_status(stream, "server_version", 15, "9.6.0", 6);
	if (rc == -1)
		return -1;
	rc = shapito_be_write_parameter_status(stream, "server_encoding", 16, "UTF-8", 6);
	if (rc == -1)
		return -1;
	rc = shapito_be_write_parameter_status(stream, "client_encoding", 16, "UTF-8", 6);
	if (rc == -1)
		return -1;
	rc = shapito_be_write_parameter_status(stream, "DateStyle", 10, "ISO", 4);
	if (rc == -1)
		return -1;
	rc = shapito_be_write_parameter_status(stream, "TimeZone", 9, "GMT", 4);
	if (rc == -1)
		return -1;
	/* ready message */
	rc = shapito_be_write_ready(stream, 'I');
	if (rc == -1)
		return -1;
	return 0;
}

static inline od_frontend_rc_t
od_frontend_setup(od_client_t *client)
{
	od_instance_t *instance = client->system->instance;
	od_route_t *route = client->route;
	shapito_stream_t *stream = &client->stream;
	shapito_stream_reset(stream);

	int rc;
	/* configure console client */
	if (route->scheme->storage->storage_type == OD_STORAGETYPE_LOCAL) {
		rc = od_frontend_setup_console(stream);
		if (rc == -1)
			return OD_FE_EATTACH;
		rc = od_write(client->io, stream);
		if (rc == -1)
			return OD_FE_ECLIENT_WRITE;
		return OD_FE_OK;
	}

	/* attach client to a server */
	od_routerstatus_t status;
	status = od_router_attach(client);
	if (status != OD_ROK)
		return OD_FE_EATTACH;

	od_server_t *server;
	server = client->server;
	od_debug(&instance->logger, "setup", client, server,
	         "attached to %s%.*s for setup",
	         server->id.id_prefix, sizeof(server->id.id),
	         server->id.id);

	/* connect to server, if necessary */
	if (server->io == NULL) {
		rc = od_backend_connect(server, "setup");
		if (rc == -1)
			return OD_FE_ESERVER_CONNECT;
	}

	/* discard last server configuration */
	if (route->scheme->pool_discard) {
		rc = od_reset_discard(client->server, "setup-discard");
		if (rc == -1)
			return OD_FE_ESERVER_CONFIGURE;
	}

	/* configure server using client startup parameters */
	rc = od_reset_configure(client->server, "setup-configure",
	                        &client->startup.params);
	if (rc == -1)
		return OD_FE_ESERVER_CONFIGURE;

	/* merge client startup parameters and server params */
	rc = shapito_parameters_merge(&client->params,
	                              &client->startup.params,
	                              &server->params);
	if (rc == -1)
		return OD_FE_ECLIENT_CONFIGURE;

	shapito_parameter_t *param;
	shapito_parameter_t *end;
	param = (shapito_parameter_t*)client->params.buf.start;
	end = (shapito_parameter_t*)client->params.buf.pos;
	while (param < end) {
		rc = shapito_be_write_parameter_status(stream,
		                                       shapito_parameter_name(param),
		                                       param->name_len,
		                                       shapito_parameter_value(param),
		                                       param->value_len);
		if (rc == -1)
			return OD_FE_ECLIENT_CONFIGURE;

		od_debug(&instance->logger, "setup", client, server,
		         "%.*s = %.*s",
		         param->name_len,
		         shapito_parameter_name(param),
		         param->value_len,
		         shapito_parameter_value(param));
		param = shapito_parameter_next(param);
	}

	/* append key data message */
	rc = shapito_be_write_backend_key_data(stream,
	                                       client->key.key_pid,
	                                       client->key.key);
	if (rc == -1)
		return OD_FE_ECLIENT_CONFIGURE;

	/* append ready message */
	rc = shapito_be_write_ready(stream, 'I');
	if (rc == -1)
		return OD_FE_ECLIENT_CONFIGURE;

	/* send to client */
	rc = od_write(client->io, stream);
	if (rc == -1)
		return OD_FE_ECLIENT_WRITE;

	/* put server back to route queue */
	od_router_detach(client);
	return OD_FE_OK;
}

static inline od_frontend_rc_t
od_frontend_copy_in(od_client_t *client)
{
	od_instance_t *instance = client->system->instance;
	od_server_t *server = client->server;

	assert(! server->is_copy);
	server->is_copy = 1;

	int rc, type;
	shapito_stream_t *stream = &client->stream;
	for (;;) {
		shapito_stream_reset(stream);
		rc = od_read(client->io, stream, UINT32_MAX);
		if (rc == -1)
			return OD_FE_ECLIENT_READ;

		od_server_stat_recv_client(server, shapito_stream_used(stream));

		type = *stream->start;
		od_debug(&instance->logger, "copy", client, server,
		         "%c", *stream->start);
		rc = od_write(server->io, stream);
		if (rc == -1)
			return OD_FE_ESERVER_WRITE;

		/* copy complete or fail */
		if (type == 'c' || type == 'f')
			break;
	}

	server->is_copy = 0;
	return OD_FE_OK;
}

static od_frontend_rc_t
od_frontend_remote(od_client_t *client)
{
	od_instance_t *instance = client->system->instance;
	od_route_t *route = client->route;

	od_server_t *server = NULL;
	shapito_stream_t *stream = &client->stream;
	for (;;)
	{
		/* client to server */
		shapito_stream_reset(stream);
		int rc;
		rc = od_read(client->io, stream, UINT32_MAX);
		if (rc == -1)
			return OD_FE_ECLIENT_READ;
		int offset = rc;
		int type = stream->start[offset];
		od_debug(&instance->logger, "main", client, server,
		         "%c", type);

		/* Query */
		if (type == 'Q' && instance->scheme.log_query) {
			uint32_t query_len;
			char *query;
			rc = shapito_be_read_query(&query, &query_len,
			                           stream->start + offset,
			                           shapito_stream_used(stream) - offset);
			if (rc == 0) {
				od_debug(&instance->logger, "main", client, server,
				         "%.*s", query_len, query);
			} else {
				od_debug(&instance->logger, "main", client, server,
				         "%s", "failed to parse Query");
			}
		}

		/* Terminate (client graceful shutdown) */
		if (type == 'X')
			break;

		/* get server connection from the route pool */
		if (server == NULL)
		{
			od_routerstatus_t status;
			status = od_router_attach(client);
			if (status != OD_ROK)
				return OD_FE_EATTACH;

			server = client->server;
			od_debug(&instance->logger, "main", client, server,
			         "attached to %s%.*s",
			         server->id.id_prefix, sizeof(server->id.id),
			         server->id.id);

			/* configure server using client startup parameters,
			 * if it has not been configured before */
			if (! od_idmgr_cmp(&server->last_client_id, &client->id))
			{
				/* connect to server, if necessary */
				if (server->io == NULL) {
					rc = od_backend_connect(server, "main");
					if (rc == -1)
						return OD_FE_ESERVER_CONNECT;
				}

				/* discard last server configuration */
				if (route->scheme->pool_discard) {
					rc = od_reset_discard(client->server, "discard");
					if (rc == -1)
						return OD_FE_ESERVER_CONFIGURE;
				}

				/* configure server using client parameters */
				rc = od_reset_configure(client->server, "configure", &client->params);
				if (rc == -1)
					return OD_FE_ESERVER_CONFIGURE;

			} else {
				assert(server->io != NULL);
				od_debug(&instance->logger, "main", client, server,
				         "previously owned, no need to reconfigure %s%.*s",
				         server->id.id_prefix, sizeof(server->id.id),
				         server->id.id);
			}
		}

		/* update request and recv stat */
		od_server_stat_request(server);
		od_server_stat_recv_client(server, shapito_stream_used(stream));

		/* extended query case */

		/* Parse or Bind */
		if (type == 'P' || type == 'B')
		{
			for (;;) {
				rc = od_read(client->io, stream, UINT32_MAX);
				if (rc == -1)
					return OD_FE_ECLIENT_READ;
				offset = rc;
				type = stream->start[offset];
				od_debug(&instance->logger, "main", client, server,
				         "%c", type);
				/* Sync */
				if (type == 'S')
					break;
			}
		}

		rc = od_write(server->io, stream);
		if (rc == -1)
			return OD_FE_ESERVER_WRITE;

		/* update server sync state */
		od_server_sync_request(server);

		shapito_stream_reset(stream);
		for (;;) {
			/* read server reply */
			for (;;) {
				rc = od_read(server->io, stream, 1000);
				if (rc >= 0)
					break;
				/* client watchdog.
				 *
				 * ensure that client has not closed
				 * the connection */
				if (! machine_timedout())
					return OD_FE_ESERVER_READ;
				if (machine_connected(client->io))
					continue;
				od_debug(&instance->logger, "watchdog", client, server,
				        "client disconnected");
				return OD_FE_ECLIENT_READ;
			}
			offset = rc;
			type = stream->start[offset];
			od_debug(&instance->logger, "main", client, server,
			         "%c", type);

			/* update server recv stats */
			od_server_stat_recv_server(server, shapito_stream_used(stream));

			/* ErrorResponse */
			if (type == 'E') {
				od_backend_error(server, "main",
				                 stream->start + offset,
				                 shapito_stream_used(stream) - offset);
			}

			/* ParameterStatus */
			if (type == 'S') {
				char *name;
				uint32_t name_len;
				char *value;
				uint32_t value_len;
				rc = shapito_fe_read_parameter(stream->start + offset,
				                               shapito_stream_used(stream) - offset,
				                               &name, &name_len,
				                               &value, &value_len);
				if (rc == -1) {
					od_error(&instance->logger, "main", client, server,
							 "failed to parse ParameterStatus message");
					return OD_FE_ESERVER_READ;
				}

				/* update server and current client parameter state */
				rc = shapito_parameters_update(&server->params, name, name_len,
				                               value, value_len);
				if (rc == -1)
					return OD_FE_ESERVER_CONFIGURE;

				rc = shapito_parameters_update(&client->params, name, name_len,
				                               value, value_len);
				if (rc == -1)
					return OD_FE_ESERVER_CONFIGURE;

				od_debug(&instance->logger, "main", client, server,
				         "%.*s = %.*s",
				         name_len, name, value_len, value);
			}

			/* ReadyForQuery */
			if (type == 'Z') {
				rc = od_backend_ready(server, "main", stream->start + offset,
				                      shapito_stream_used(stream) - offset);
				if (rc == -1)
					return OD_FE_ECLIENT_READ;

				/* force buffer flush to client */
				rc = od_write(client->io, stream);
				if (rc == -1)
					return OD_FE_ECLIENT_WRITE;

				/* handle transaction pooling */
				if (route->scheme->pool == OD_POOLING_TRANSACTION) {
					if (! server->is_transaction) {
						/* cleanup server */
						rc = od_reset(server);
						if (rc == -1)
							return OD_FE_ESERVER_WRITE;
						/* push server connection back to route pool */
						od_router_detach(client);
						server = NULL;
					}
				}
				break;
			}

			/* CopyInResponse */
			if (type == 'G') {
				/* force buffer flush to client */
				rc = od_write(client->io, stream);
				if (rc == -1)
					return OD_FE_ECLIENT_WRITE;

				/* switch to CopyIn mode */
				rc = od_frontend_copy_in(client);
				if (rc != OD_FE_OK)
					return rc;
				continue;
			}
			/* CopyOutResponse */
			if (type == 'H') {
				assert(! server->is_copy);
				server->is_copy = 1;
			}
			/* CopyDone */
			if (type == 'c') {
				server->is_copy = 0;
			}

			/* server pipelining buffer flush */
			if (shapito_stream_used(stream) >= instance->scheme.server_pipelining)
			{
				rc = od_write(client->io, stream);
				if (rc == -1)
					return OD_FE_ECLIENT_WRITE;
				shapito_stream_reset(stream);
			}
		}
	}
	return OD_FE_OK;
}

static od_frontend_rc_t
od_frontend_local(od_client_t *client)
{
	od_instance_t *instance = client->system->instance;
	int rc;

	shapito_stream_t *stream = &client->stream;
	for (;;)
	{
		/* read client request */
		shapito_stream_reset(stream);
		rc = od_read(client->io, stream, UINT32_MAX);
		if (rc == -1)
			return OD_FE_ECLIENT_READ;
		int offset = rc;
		int type = stream->start[offset];
		od_debug(&instance->logger, "local", client, NULL,
		         "%c", type);

		/* Terminate */
		if (type == 'X')
			break;

		/* Query */
		if (type == 'Q') {
			od_consolestatus_t cs;
			cs = od_console_request(client, stream->start + offset,
			                        shapito_stream_used(stream) - offset);
			if (cs == OD_CERROR) {
			}
			rc = od_write(client->io, stream);
			if (rc == -1)
				return OD_FE_ECLIENT_WRITE;
			continue;
		}

		/* unsupported */
		od_error(&instance->logger, "local", client, NULL,
		         "unsupported request '%c'",
		         type);
		od_frontend_error(client, SHAPITO_FEATURE_NOT_SUPPORTED,
		                  "unsupported request '%c'", type);
		shapito_stream_reset(stream);
		rc = shapito_be_write_ready(stream, 'I');
		if (rc == -1)
			return OD_FE_ECLIENT_WRITE;
		rc = od_write(client->io, stream);
		if (rc == -1)
			return OD_FE_ECLIENT_WRITE;
	}
	return OD_FE_OK;
}

static void
od_frontend_cleanup(od_client_t *client, char *context,
                    od_frontend_rc_t status)
{
	od_instance_t *instance = client->system->instance;
	int rc;

	od_server_t *server = client->server;
	switch (status) {
	case OD_FE_EATTACH:
		assert(server == NULL);
		assert(client->route != NULL);
		od_frontend_error(client, SHAPITO_CONNECTION_FAILURE,
		                  "failed to get remote server connection");
		/* detach client from route */
		od_unroute(client);
		break;

	case OD_FE_OK:
		/* graceful disconnect */
		if (instance->scheme.log_session) {
			od_log(&instance->logger, context, client, server,
			       "client disconnected");
		}
		if (! client->server) {
			od_unroute(client);
			break;
		}
		rc = od_reset(server);
		if (rc != 1) {
			/* close backend connection */
			od_router_close_and_unroute(client);
			break;
		}
		/* push server to router server pool */
		od_router_detach_and_unroute(client);
		break;

	case OD_FE_ECLIENT_READ:
	case OD_FE_ECLIENT_WRITE:
		/* close client connection and reuse server
		 * link in case of client errors */
		od_log(&instance->logger, context, client, server,
		       "client disconnected (read/write error): %s",
		       machine_error(client->io));
		if (! client->server) {
			od_unroute(client);
			break;
		}
		rc = od_reset(server);
		if (rc != 1) {
			/* close backend connection */
			od_router_close_and_unroute(client);
			break;
		}
		/* push server to router server pool */
		od_router_detach_and_unroute(client);
		break;

	case OD_FE_ECLIENT_CONFIGURE:
		/* close client connection and reuse server
		 * link in case of client errors during setup */
		od_log(&instance->logger, context, client, server,
		       "client disconnected (read/write error): %s",
		       machine_error(client->io));
		if (! client->server) {
			od_unroute(client);
			break;
		}
		od_frontend_error(client, SHAPITO_CONNECTION_FAILURE,
		                  "client %s%.*s configuration error",
		                  client->id.id_prefix,
		                  sizeof(client->id.id), client->id.id);
		/* push server to router server pool */
		od_router_detach_and_unroute(client);
		break;

	case OD_FE_ESERVER_CONNECT:
	{
		/* server attached to client and connection failed */
		od_route_t *route = client->route;
		if (route->scheme->client_fwd_error && server->stats.count_error) {
			/* forward server error to client */
			od_frontend_error_fwd(client);
		} else {
			od_frontend_error(client, SHAPITO_CONNECTION_FAILURE,
			                  "failed to connect to remote server %s%.*s",
			                  server->id.id_prefix,
			                  sizeof(server->id.id), server->id.id);
		}
		/* close backend connection */
		od_router_close_and_unroute(client);
		break;
	}

	case OD_FE_ESERVER_CONFIGURE:
		od_log(&instance->logger, context, client, server,
		       "server disconnected (server configure error)");
		od_frontend_error(client, SHAPITO_CONNECTION_FAILURE,
		                  "failed to configure remote server %s%.*s",
		                  server->id.id_prefix,
		                  sizeof(server->id.id), server->id.id);
		/* close backend connection */
		od_router_close_and_unroute(client);
		break;

	case OD_FE_ESERVER_READ:
	case OD_FE_ESERVER_WRITE:
		/* close client connection and close server
		 * connection in case of server errors */
		od_log(&instance->logger, context, client, server,
		       "server disconnected (read/write error): %s",
		       machine_error(server->io));
		od_frontend_error(client, SHAPITO_CONNECTION_FAILURE,
		                  "remote server read/write error %s%.*s",
		                  server->id.id_prefix,
		                  sizeof(server->id.id), server->id.id);
		/* close backend connection */
		od_router_close_and_unroute(client);
		break;

	case OD_FE_UNDEF:
		assert(0);
		break;
	}
}

void od_frontend(void *arg)
{
	od_client_t *client = arg;
	od_instance_t *instance = client->system->instance;

	/* log client connection */
	if (instance->scheme.log_session) {
		char peer[128];
		od_getpeername(client->io, peer, sizeof(peer), 1, 1);
		od_log(&instance->logger, "startup", client, NULL,
		       "new client connection %s",
		       peer);
	}

	/* attach client io to relay machine event loop */
	int rc;
	rc = machine_io_attach(client->io);
	if (rc == -1) {
		od_error(&instance->logger, "startup", client, NULL,
		         "failed to transfer client io");
		machine_close(client->io);
		od_client_free(client);
		return;
	}

	/* handle startup */
	rc = od_frontend_startup(client);
	if (rc == -1) {
		od_frontend_close(client);
		return;
	}

	/* handle cancel request */
	if (client->startup.is_cancel) {
		od_debug(&instance->logger, "startup", client, NULL,
		         "cancel request");
		od_router_cancel(client);
		od_frontend_close(client);
		return;
	}

	/* set client backend key */
	od_frontend_key(client);

	/* route client */
	od_routerstatus_t status;
	status = od_route(client);
	switch (status) {
	case OD_RERROR:
		od_error(&instance->logger, "startup", client, NULL,
		         "routing failed, closing");
		od_frontend_error(client, SHAPITO_SYSTEM_ERROR,
		                  "client routing failed");
		od_frontend_close(client);
		return;
	case OD_RERROR_NOT_FOUND:
		od_error(&instance->logger, "startup", client, NULL,
		         "route '%s.%s' not matched, closing",
		         shapito_parameter_value(client->startup.database),
		         shapito_parameter_value(client->startup.user));
		od_frontend_error(client, SHAPITO_UNDEFINED_DATABASE,
		                  "route is not matched");
		od_frontend_close(client);
		return;
	case OD_RERROR_LIMIT:
		od_error(&instance->logger, "startup", client, NULL,
		         "route connection limit reached, closing");
		od_frontend_error(client, SHAPITO_TOO_MANY_CONNECTIONS,
		                  "too many connections");
		od_frontend_close(client);
		return;
	case OD_ROK:
	{
		od_route_t *route = client->route;
		od_log(&instance->logger, "startup", client, NULL,
		       "route '%s.%s' to '%s.%s'",
		       shapito_parameter_value(client->startup.database),
		       shapito_parameter_value(client->startup.user),
		       route->scheme->db_name,
		       route->scheme->user_name);
		break;
	}
	default:
		assert(0);
		break;
	}

	/* client authentication */
	rc = od_auth_frontend(client);
	if (rc == -1) {
		od_unroute(client);
		od_frontend_close(client);
		return;
	}

	/* configure client parameters and send ready */
	od_frontend_rc_t frontend_rc;
	frontend_rc = od_frontend_setup(client);
	if (frontend_rc != OD_FE_OK) {
		od_frontend_cleanup(client, "setup", frontend_rc);
		od_frontend_close(client);
		return;
	}

	/* main */
	od_route_t *route = client->route;
	switch (route->scheme->storage->storage_type) {
	case OD_STORAGETYPE_REMOTE:
		frontend_rc = od_frontend_remote(client);
		break;
	case OD_STORAGETYPE_LOCAL:
		frontend_rc = od_frontend_local(client);
		break;
	}

	/* reset client and server state */
	od_frontend_cleanup(client, "main", frontend_rc);

	/* close frontend connection */
	od_frontend_close(client);
}
