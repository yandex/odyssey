
/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
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

#include "od_macro.h"
#include "od_version.h"
#include "od_list.h"
#include "od_pid.h"
#include "od_id.h"
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

#include "od_router.h"
#include "od_pooler.h"
#include "od_relay.h"
#include "od_frontend.h"
#include "od_backend.h"
#include "od_auth.h"
#include "od_tls.h"

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

int od_frontend_error(od_client_t *client, char *code, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	char msg[512];
	int msg_len;
	msg_len = snprintf(msg, sizeof(msg), "odissey: ");
	msg_len += vsnprintf(msg + msg_len, sizeof(msg) - msg_len, fmt, args);
	va_end(args);
	so_stream_t *stream = &client->stream;
	so_stream_reset(stream);
	int rc;
	rc = so_bewrite_error(stream, code, msg, msg_len);
	if (rc == -1)
		return -1;
	rc = od_write(client->io, stream);
	return rc;
}

static int
od_frontend_startup_read(od_client_t *client)
{
	od_instance_t *instance = client->system->instance;

	so_stream_t *stream = &client->stream;
	so_stream_reset(stream);
	for (;;) {
		uint32_t pos_size = so_stream_used(stream);
		uint8_t *pos_data = stream->s;
		uint32_t len;
		int to_read;
		to_read = so_read_startup(&len, &pos_data, &pos_size);
		if (to_read == 0)
			break;
		if (to_read == -1) {
			od_error_client(&instance->log, &client->id, "startup",
			                "failed to read startup packet, closing");
			return -1;
		}
		int rc = so_stream_ensure(stream, to_read);
		if (rc == -1)
			return -1;
		rc = machine_read(client->io, (char*)stream->p, to_read, UINT32_MAX);
		if (rc == -1) {
			od_error_client(&instance->log, &client->id, "startup",
			                "read error: %s",
			                machine_error(client->io));
			return -1;
		}
		so_stream_advance(stream, to_read);
	}
	return 0;
}

static int
od_frontend_startup(od_client_t *client)
{
	od_instance_t *instance = client->system->instance;
	od_pooler_t *pooler = client->system->pooler;

	int rc;
	rc = od_frontend_startup_read(client);
	if (rc == -1)
		return -1;
	so_stream_t *stream = &client->stream;
	rc = so_beread_startup(&client->startup,
	                       stream->s,
	                       so_stream_used(stream));
	if (rc == -1)
		goto error;

	/* client ssl request */
	rc = od_tls_frontend_accept(client, &instance->log,
	                            &instance->scheme,
	                            pooler->tls);
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
	rc = so_beread_startup(&client->startup,
	                       stream->s,
	                       so_stream_used(stream));
	if (rc == -1)
		goto error;
	return 0;

error:
	od_error_client(&instance->log, &client->id, "startup",
	                "incorrect startup packet");
	od_frontend_error(client, SO_ERROR_PROTOCOL_VIOLATION,
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

	so_stream_t *stream = &client->stream;
	so_stream_reset(stream);
	int rc;
	rc = so_bewrite_backend_key_data(stream, client->key.key_pid,
	                                 client->key.key);
	if (rc == -1)
		return -1;
	rc = so_bewrite_parameter_status(stream, "", 1, "", 1);
	if (rc == -1)
		return -1;
	rc = od_write(client->io, stream);
	if (rc == -1) {
		od_error_client(&instance->log, &client->id, "setup",
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

	so_stream_t *stream = &client->stream;
	so_stream_reset(stream);
	int rc;
	rc = so_bewrite_ready(stream, 'I');
	if (rc == -1)
		return -1;
	rc = od_write(client->io, stream);
	if (rc == -1) {
		od_error_client(&instance->log, &client->id,
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
	so_stream_t *stream = &client->stream;
	for (;;) {
		so_stream_reset(stream);
		rc = od_read(client->io, stream, UINT32_MAX);
		if (rc == -1)
			return OD_RS_ECLIENT_READ;
		type = *stream->s;
		od_debug_client(&instance->log, &client->id, "copy",
		                "%c", *stream->s);
		rc = od_write(server->io, stream);
		if (rc == -1)
			return OD_RS_ESERVER_WRITE;

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
	int rc;

	od_server_t *server = NULL;
	so_stream_t *stream = &client->stream;
	for (;;)
	{
		/* client to server */
		so_stream_reset(stream);
		rc = od_read(client->io, stream, UINT32_MAX);
		if (rc == -1)
			return OD_RS_ECLIENT_READ;
		int offset = rc;
		int type = stream->s[offset];
		od_debug_client(&instance->log, &client->id, NULL,
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
			od_debug_client(&instance->log, &client->id, NULL,
			                "attached to s%.*s",
			                sizeof(server->id.id),
			                server->id.id);

			/* configure server using client startup parameters,
			 * if it has not been configured before. */
			if (od_idmgr_cmp(&server->last_client_id, &client->id)) {
				assert(server->io != NULL);
				od_debug_client(&instance->log, &client->id, NULL,
				                "previously owned, no need to reconfigure s%.*s",
				                sizeof(server->id.id),
				                server->id.id);
			} else {
				od_route_t *route = client->route;

				/* connect to server, if necessary */
				if (server->io == NULL) {
					rc = od_backend_connect(server);
					if (rc == -1)
						return OD_RS_ESERVER_CONNECT;
				} else {
					/* discard last server configuration */
					if (route->scheme->discard) {
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

		rc = od_write(server->io, stream);
		if (rc == -1)
			return OD_RS_ESERVER_WRITE;

		server->count_request++;

		so_stream_reset(stream);
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
				od_debug_server(&instance->log, &server->id, "watchdog",
				                "client disconnected");
				return OD_RS_ECLIENT_READ;
			}
			offset = rc;
			type = stream->s[offset];
			od_debug_server(&instance->log, &server->id, NULL,
			                "%c", type);

			/* ErrorResponse */
			if (type == 'E') {
				od_backend_error(server, NULL,
				                 stream->s + offset,
				                 so_stream_used(stream) - offset);
			}

			/* ReadyForQuery */
			if (type == 'Z') {
				rc = od_backend_ready(server, stream->s + offset,
				                      so_stream_used(stream) - offset);
				if (rc == -1)
					return OD_RS_ECLIENT_READ;

				/* force buffer flush to client */
				rc = od_write(client->io, stream);
				if (rc == -1)
					return OD_RS_ECLIENT_WRITE;

				/* transaction pooling */
				if (instance->scheme.pooling_mode == OD_PTRANSACTION) {
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
			if (so_stream_used(stream) >= instance->scheme.server_pipelining) {
				rc = od_write(client->io, stream);
				if (rc == -1)
					return OD_RS_ECLIENT_WRITE;
				so_stream_reset(stream);
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

	so_stream_t *stream = &client->stream;
	for (;;)
	{
		/* read client request */
		so_stream_reset(stream);
		rc = od_read(client->io, stream, UINT32_MAX);
		if (rc == -1)
			return OD_RS_ECLIENT_READ;
		int offset = rc;
		int type = stream->s[offset];
		od_debug_client(&instance->log, &client->id, NULL,
		                "%c", type);

		/* Terminate */
		if (type == 'X')
			break;

		/* Query */
		if (type == 'Q') {
			/* todo: */
			/* send request to storage */
				/* wait for reply */
			/* send reply to client */
		}

		/* unsupported */
		od_error_client(&instance->log, &client->id, "local",
		                "unsupported request '%c'",
		                type);

		od_frontend_error(client, SO_ERROR_FEATURE_NOT_SUPPORTED,
		                  "c%.*s: unsupported request '%c'",
		                  sizeof(client->id.id),
		                  client->id.id, type);

		so_stream_reset(stream);
		rc = so_bewrite_ready(stream, 'I');
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

	char peer[128];
	od_getpeername(client->io, peer, sizeof(peer));
	od_log_client(&instance->log, &client->id, NULL,
	              "new client connection %s",
	              peer);

	/* attach client io to relay machine event loop */
	int rc;
	rc = machine_io_attach(client->io);
	if (rc == -1) {
		od_error_client(&instance->log, &client->id, NULL,
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
		od_debug_client(&instance->log, &client->id, NULL,
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

	/* client authentication */
	rc = od_auth_frontend(client);
	if (rc == -1) {
		od_frontend_close(client);
		return;
	}

	/* set client backend options and the key */
	rc = od_frontend_setup(client);
	if (rc == -1) {
		od_frontend_close(client);
		return;
	}

	/* notify client that we are ready */
	rc = od_frontend_ready(client);
	if (rc == -1) {
		od_frontend_close(client);
		return;
	}

	/* route client */
	od_routerstatus_t status;
	status = od_route(client);
	switch (status) {
	case OD_RERROR:
		od_error_client(&instance->log, &client->id, NULL,
		                "routing failed, closing");
		od_frontend_error(client, SO_ERROR_SYSTEM_ERROR,
		                  "client routing failed");
		od_frontend_close(client);
		return;
	case OD_RERROR_NOT_FOUND:
		od_error_client(&instance->log, &client->id, NULL,
		                "database route '%s' is not declared, closing",
		                so_parameter_value(client->startup.database));
		od_frontend_error(client, SO_ERROR_UNDEFINED_DATABASE,
		                  "database route is not declared");
		od_frontend_close(client);
		return;
	case OD_RERROR_TIMEDOUT:
		od_error_client(&instance->log, &client->id, NULL,
		                "route connection timedout, closing");
		od_frontend_error(client, SO_ERROR_TOO_MANY_CONNECTIONS,
		                  "connection timedout");
		od_frontend_close(client);
		return;
	case OD_RERROR_LIMIT:
		od_error_client(&instance->log, &client->id, NULL,
		                "route connection limit reached, closing");
		od_frontend_error(client, SO_ERROR_TOO_MANY_CONNECTIONS,
		                  "too many connections");
		od_frontend_close(client);
		return;
	case OD_ROK:
	{
		od_route_t *route = client->route;
		od_debug_client(&instance->log, &client->id, NULL,
		                "route to '%s' (using '%s' storage)",
		                route->scheme->target,
		                route->scheme->storage->name);
		break;
	}
	}

	/* client main */
	od_route_t *route = client->route;
	switch (route->scheme->storage->storage_type) {
	case OD_SREMOTE:
		rc = od_frontend_remote(client);
		break;
	case OD_SLOCAL:
		rc = od_frontend_local(client);
		break;
	}

	/* cleanup */
	od_server_t *server = client->server;
	switch (rc) {
	case OD_RS_EATTACH:
		assert(server == NULL);
		assert(client->route != NULL);
		od_frontend_error(client, SO_ERROR_CONNECTION_FAILURE,
		                  "c%.*s: failed to get remote server connection",
		                  sizeof(client->id.id),
		                  client->id.id);
		/* detach client from route */
		od_unroute(client);
		break;

	case OD_RS_OK:
		/* graceful disconnect */
		od_log_client(&instance->log, &client->id, NULL,
		              "disconnected");
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
		od_log_client(&instance->log, &client->id, NULL,
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
		od_frontend_error(client, SO_ERROR_CONNECTION_FAILURE,
		                  "s%.*s: failed to connect to remote server",
		                  sizeof(server->id.id),
		                  server->id.id);
		/* close backend connection */
		od_router_close_and_unroute(client);
		break;

	case OD_RS_ESERVER_CONFIGURE:
		od_log_server(&instance->log, &server->id, NULL,
		              "disconnected (server configure error)");
		od_frontend_error(client, SO_ERROR_CONNECTION_FAILURE,
		                  "s%.*s: failed to configure remote server",
		                  sizeof(server->id.id),
		                  server->id.id);
		/* close backend connection */
		od_router_close_and_unroute(client);
		break;

	case OD_RS_ESERVER_READ:
	case OD_RS_ESERVER_WRITE:
		/* close client connection and close server
		 * connection in case of server errors */
		od_log_server(&instance->log, &server->id, NULL,
		              "disconnected (read/write error): %s",
		              machine_error(server->io));
		od_frontend_error(client, SO_ERROR_CONNECTION_FAILURE,
		                  "s%.*s: remote server read/write error",
		                  sizeof(server->id.id),
		                  server->id.id);
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
