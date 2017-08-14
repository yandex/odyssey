
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
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/log_file.h"
#include "sources/log_system.h"
#include "sources/logger.h"
#include "sources/daemon.h"
#include "sources/scheme.h"
#include "sources/scheme_mgr.h"
#include "sources/config.h"
#include "sources/msg.h"
#include "sources/system.h"
#include "sources/instance.h"
#include "sources/server.h"
#include "sources/server_pool.h"
#include "sources/client.h"
#include "sources/client_pool.h"
#include "sources/route_id.h"
#include "sources/route.h"
#include "sources/route_pool.h"
#include "sources/io.h"
#include "sources/router.h"
#include "sources/pooler.h"
#include "sources/relay.h"
#include "sources/frontend.h"
#include "sources/backend.h"
#include "sources/tls.h"
#include "sources/auth.h"
#include "sources/console.h"

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
od_frontend_verror(od_client_t *client, char *code, char *fmt, va_list args)
{
	char msg[512];
	int msg_len;
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
	va_list args;
	va_start(args, fmt);
	int rc;
	rc = od_frontend_verror(client, code, fmt, args);
	va_end(args);
	if (rc == -1)
		return -1;
	shapito_stream_t *stream = &client->stream;
	shapito_stream_reset(stream);
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
			od_error_client(&instance->logger, &client->id, "startup",
			                "failed to read startup packet, closing");
			return -1;
		}
		int rc = shapito_stream_ensure(stream, to_read);
		if (rc == -1)
			return -1;
		rc = machine_read(client->io, stream->pos, to_read, UINT32_MAX);
		if (rc == -1) {
			od_error_client(&instance->logger, &client->id, "startup",
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
	                            &instance->scheme,
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
	od_error_client(&instance->logger, &client->id, "startup",
	                "incorrect startup packet");
	od_frontend_error(client, SHAPITO_PROTOCOL_VIOLATION,
	                  "bad startup packet");
	return -1;
}

static inline void
od_frontend_key(od_client_t *client)
{
	client->key.key_pid = *(uint32_t*)client->id.id;
	client->key.key = 1 + rand();
}

static inline int
od_frontend_setup(od_client_t *client)
{
	od_instance_t *instance = client->system->instance;

	shapito_stream_t *stream = &client->stream;
	shapito_stream_reset(stream);
	int rc;
	rc = shapito_be_write_backend_key_data(stream, client->key.key_pid,
	                                       client->key.key);
	if (rc == -1)
		return -1;
	rc = shapito_be_write_parameter_status(stream, "", 1, "", 1);
	if (rc == -1)
		return -1;
	rc = od_write(client->io, stream);
	if (rc == -1) {
		od_error_client(&instance->logger, &client->id, "setup",
		                "write error: %s",
		                machine_error(client->io));
		return -1;
	}
	return 0;
}

static inline int
od_frontend_ready(od_client_t *client)
{
	od_instance_t *instance = client->system->instance;

	shapito_stream_t *stream = &client->stream;
	shapito_stream_reset(stream);
	int rc;
	rc = shapito_be_write_ready(stream, 'I');
	if (rc == -1)
		return -1;
	rc = od_write(client->io, stream);
	if (rc == -1) {
		od_error_client(&instance->logger, &client->id,
		                "write error: %s",
		                machine_error(client->io));
		return -1;
	}
	return 0;
}

enum {
	OD_RS_UNDEF,
	OD_RS_OK,
	OD_RS_EATTACH,
	OD_RS_ESERVER_CONNECT,
	OD_RS_ESERVER_CONFIGURE,
	OD_RS_ESERVER_READ,
	OD_RS_ESERVER_WRITE,
	OD_RS_ECLIENT_READ,
	OD_RS_ECLIENT_WRITE
};

static inline int
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
			return OD_RS_ECLIENT_READ;

		od_server_stat_recv(server, shapito_stream_used(stream));

		type = *stream->start;
		od_debug_client(&instance->logger, &client->id, "copy",
		                "%c", *stream->start);
		rc = od_write(server->io, stream);
		if (rc == -1)
			return OD_RS_ESERVER_WRITE;

		od_server_stat_sent(server, shapito_stream_used(stream));

		/* copy complete or fail */
		if (type == 'c' || type == 'f')
			break;
	}

	server->is_copy = 0;
	return OD_RS_OK;
}

static int
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
			return OD_RS_ECLIENT_READ;
		int offset = rc;
		int type = stream->start[offset];
		od_debug_client(&instance->logger, &client->id, NULL,
		                "%c", type);

		/* Terminate (client graceful shutdown) */
		if (type == 'X')
			break;

		/* get server connection from the route pool */
		if (server == NULL)
		{
			od_routerstatus_t status;
			status = od_router_attach(client);
			if (status != OD_ROK)
				return OD_RS_EATTACH;
			server = client->server;
			od_debug_client(&instance->logger, &client->id, NULL,
			                "attached to %s%.*s",
			                server->id.id_prefix, sizeof(server->id.id),
			                server->id.id);

			/* configure server using client startup parameters,
			 * if it has not been configured before. */
			if (od_idmgr_cmp(&server->last_client_id, &client->id)) {
				assert(server->io != NULL);
				od_debug_client(&instance->logger, &client->id, NULL,
				                "previously owned, no need to reconfigure %s%.*s",
				                server->id.id_prefix, sizeof(server->id.id),
				                server->id.id);
			} else {
				/* connect to server, if necessary */
				if (server->io == NULL) {
					rc = od_backend_connect(server);
					if (rc == -1)
						return OD_RS_ESERVER_CONNECT;
				} else {
					/* discard last server configuration */
					if (route->scheme->pool_discard) {
						rc = od_backend_discard(client->server);
						if (rc == -1)
							return OD_RS_ESERVER_CONFIGURE;
					}
				}

				/* set client parameters */
				rc = od_backend_configure(client->server, &client->startup);
				if (rc == -1)
					return OD_RS_ESERVER_CONFIGURE;
			}
		}

		/* update request and recv stat */
		od_server_stat_request(server);
		od_server_stat_recv(server, shapito_stream_used(stream));

		rc = od_write(server->io, stream);
		if (rc == -1)
			return OD_RS_ESERVER_WRITE;

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
					return OD_RS_ESERVER_READ;
				if (machine_connected(client->io))
					continue;
				od_debug_server(&instance->logger, &server->id, "watchdog",
				                "client disconnected");
				return OD_RS_ECLIENT_READ;
			}
			offset = rc;
			type = stream->start[offset];
			od_debug_server(&instance->logger, &server->id, NULL,
			                "%c", type);


			/* ErrorResponse */
			if (type == 'E') {
				od_backend_error(server, NULL,
				                 stream->start + offset,
				                 shapito_stream_used(stream) - offset);
			}

			/* ReadyForQuery */
			if (type == 'Z') {
				rc = od_backend_ready(server, NULL, stream->start + offset,
				                      shapito_stream_used(stream) - offset);
				if (rc == -1)
					return OD_RS_ECLIENT_READ;

				/* force buffer flush to client */
				rc = od_write(client->io, stream);
				if (rc == -1)
					return OD_RS_ECLIENT_WRITE;

				/* handle transaction pooling */
				if (route->scheme->pool == OD_POOLING_TRANSACTION) {
					if (! server->is_transaction) {
						/* cleanup server */
						rc = od_backend_reset(server);
						if (rc == -1)
							return OD_RS_ESERVER_WRITE;
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
					return OD_RS_ECLIENT_WRITE;

				/* update stats */
				od_server_stat_sent(server, shapito_stream_used(stream));

				/* switch to CopyIn mode */
				rc = od_frontend_copy_in(client);
				if (rc != OD_RS_OK)
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
					return OD_RS_ECLIENT_WRITE;
				/* update stats */
				od_server_stat_sent(server, shapito_stream_used(stream));
				shapito_stream_reset(stream);
			}
		}
	}
	return OD_RS_OK;
}

static int
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
			return OD_RS_ECLIENT_READ;
		int offset = rc;
		int type = stream->start[offset];
		od_debug_client(&instance->logger, &client->id, NULL,
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
				return OD_RS_ECLIENT_WRITE;
			continue;
		}

		/* unsupported */
		od_error_client(&instance->logger, &client->id, "local",
		                "unsupported request '%c'",
		                type);
		od_frontend_error(client, SHAPITO_FEATURE_NOT_SUPPORTED,
		                  "unsupported request '%c'", type);
		shapito_stream_reset(stream);
		rc = shapito_be_write_ready(stream, 'I');
		if (rc == -1)
			return OD_RS_ECLIENT_WRITE;
		rc = od_write(client->io, stream);
		if (rc == -1)
			return OD_RS_ECLIENT_WRITE;
	}
	return OD_RS_OK;
}

void od_frontend(void *arg)
{
	od_client_t *client = arg;
	od_instance_t *instance = client->system->instance;

	/* log client connection */
	if (instance->scheme.log_session) {
		char peer[128];
		od_getpeername(client->io, peer, sizeof(peer));
		od_log_client(&instance->logger, &client->id, NULL,
		              "new client connection %s",
		              peer);
	}

	/* attach client io to relay machine event loop */
	int rc;
	rc = machine_io_attach(client->io);
	if (rc == -1) {
		od_error_client(&instance->logger, &client->id, NULL,
		                "failed to transfer client io");
		machine_close(client->io);
		od_client_free(client);
		return;
	}

	/* client startup */
	rc = od_frontend_startup(client);
	if (rc == -1) {
		od_frontend_close(client);
		return;
	}

	/* client cancel request */
	if (client->startup.is_cancel) {
		od_debug_client(&instance->logger, &client->id, NULL,
		                "cancel request");
		od_router_cancel(client);
		od_frontend_close(client);
		return;
	}

	/* Generate backend key for the client.
	 *
	 * This key will be used to identify a server by
	 * user cancel requests. The key must be regenerated
	 * for each new client-server assignment, to avoid
	 * possibility of cancelling requests by a previous
	 * server owners.
	 */
	od_frontend_key(client);

	/* route client */
	od_routerstatus_t status;
	status = od_route(client);
	switch (status) {
	case OD_RERROR:
		od_error_client(&instance->logger, &client->id, NULL,
		                "routing failed, closing");
		od_frontend_error(client, SHAPITO_SYSTEM_ERROR,
		                  "client routing failed");
		od_frontend_close(client);
		return;
	case OD_RERROR_NOT_FOUND:
		od_error_client(&instance->logger, &client->id, NULL,
		                "route '%s.%s' not matched, closing",
		                shapito_parameter_value(client->startup.database),
		                shapito_parameter_value(client->startup.user));
		od_frontend_error(client, SHAPITO_UNDEFINED_DATABASE,
		                  "route is not matched");
		od_frontend_close(client);
		return;
	case OD_RERROR_LIMIT:
		od_error_client(&instance->logger, &client->id, NULL,
		                "route connection limit reached, closing");
		od_frontend_error(client, SHAPITO_TOO_MANY_CONNECTIONS,
		                  "too many connections");
		od_frontend_close(client);
		return;
	case OD_ROK:
	{
		od_route_t *route = client->route;
		od_debug_client(&instance->logger, &client->id, NULL,
		                "route to '%s.%s' (using '%s' storage)",
		                route->scheme->db_name,
		                route->scheme->user_name,
		                route->scheme->storage->name);
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

	/* set client backend options and the key */
	rc = od_frontend_setup(client);
	if (rc == -1) {
		od_unroute(client);
		od_frontend_close(client);
		return;
	}

	/* notify client that we are ready */
	rc = od_frontend_ready(client);
	if (rc == -1) {
		od_unroute(client);
		od_frontend_close(client);
		return;
	}

	/* client main */
	od_route_t *route = client->route;
	switch (route->scheme->storage->storage_type) {
	case OD_STORAGETYPE_REMOTE:
		rc = od_frontend_remote(client);
		break;
	case OD_STORAGETYPE_LOCAL:
		rc = od_frontend_local(client);
		break;
	}

	/* cleanup */
	od_server_t *server = client->server;
	switch (rc) {
	case OD_RS_EATTACH:
		assert(server == NULL);
		assert(client->route != NULL);
		od_frontend_error(client, SHAPITO_CONNECTION_FAILURE,
		                  "failed to get remote server connection");
		/* detach client from route */
		od_unroute(client);
		break;

	case OD_RS_OK:
		/* graceful disconnect */
		if (instance->scheme.log_session) {
			od_log_client(&instance->logger, &client->id, NULL,
			              "disconnected");
		}
		if (! client->server) {
			od_unroute(client);
			break;
		}
		rc = od_backend_reset(server);
		if (rc != 1) {
			/* close backend connection */
			od_router_close_and_unroute(client);
			break;
		}
		/* push server to router server pool */
		od_router_detach_and_unroute(client);
		break;

	case OD_RS_ECLIENT_READ:
	case OD_RS_ECLIENT_WRITE:
		/* close client connection and reuse server
		 * link in case of client errors */
		od_log_client(&instance->logger, &client->id, NULL,
		              "disconnected (read/write error): %s",
		              machine_error(client->io));
		if (! client->server) {
			od_unroute(client);
			break;
		}
		rc = od_backend_reset(server);
		if (rc != 1) {
			/* close backend connection */
			od_router_close_and_unroute(client);
			break;
		}
		/* push server to router server pool */
		od_router_detach_and_unroute(client);
		break;

	case OD_RS_ESERVER_CONNECT:
		/* server attached to client and connection failed */
		od_frontend_error(client, SHAPITO_CONNECTION_FAILURE,
		                  "failed to connect to remote server %s%.*s",
		                  server->id.id_prefix,
		                  sizeof(server->id.id), server->id.id);
		/* close backend connection */
		od_router_close_and_unroute(client);
		break;

	case OD_RS_ESERVER_CONFIGURE:
		od_log_server(&instance->logger, &server->id, NULL,
		              "disconnected (server configure error)");
		od_frontend_error(client, SHAPITO_CONNECTION_FAILURE,
		                  "failed to configure remote server %s%.*s",
		                  server->id.id_prefix,
		                  sizeof(server->id.id), server->id.id);
		/* close backend connection */
		od_router_close_and_unroute(client);
		break;

	case OD_RS_ESERVER_READ:
	case OD_RS_ESERVER_WRITE:
		/* close client connection and close server
		 * connection in case of server errors */
		od_log_server(&instance->logger, &server->id, NULL,
		              "disconnected (read/write error): %s",
		              machine_error(server->io));
		od_frontend_error(client, SHAPITO_CONNECTION_FAILURE,
		                  "remote server read/write error %s%.*s",
		                  server->id.id_prefix,
		                  sizeof(server->id.id), server->id.id);
		/* close backend connection */
		od_router_close_and_unroute(client);
		break;

	case OD_RS_UNDEF:
		assert(0);
		break;
	}

	/* close frontend connection */
	od_frontend_close(client);
}
