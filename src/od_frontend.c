
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

void od_frontend_close(od_client_t *client)
{
#if 0
	od_pooler_t *pooler = client->pooler;
	if (client->route) {
		od_route_t *route = client->route;
		od_clientpool_set(&route->client_pool, client, OD_CUNDEF);
		client->route = NULL;
	}
#endif
	if (client->io) {
		machine_close(client->io);
		machine_io_free(client->io);
		client->io = NULL;
	}
#if 0
	od_clientlist_unlink(&pooler->client_list, client);
#endif
	od_client_free(client);
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
			od_error(&instance->log, client->io,
			         "C (startup): bad startup packet");
			return -1;
		}
		int rc = so_stream_ensure(stream, to_read);
		if (rc == -1)
			return -1;
		rc = machine_read(client->io, (char*)stream->p, to_read, UINT32_MAX);
		if (rc < 0) {
			od_error(&instance->log, client->io,
			         "C (startup): read error: %s",
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
	int rc;
	rc = od_frontend_startup_read(client);
	if (rc == -1)
		return -1;
	so_stream_t *stream = &client->stream;
	rc = so_beread_startup(&client->startup,
	                       stream->s,
	                       so_stream_used(stream));
	if (rc == -1)
		return -1;

#if 0
	/* client ssl request */
	rc = od_tlsfe_accept(pooler->env, client->io, pooler->tls,
	                     &client->stream,
	                     &pooler->od->log, "C",
	                     &pooler->od->scheme,
	                     &client->startup);
	if (rc == -1)
		return -1;
#endif
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
		return -1;
	return 0;
}

static inline void
od_frontend_key(od_client_t *client)
{
	client->key.key_pid = client->id;
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
		od_error(&instance->log, client->io, "C (setup): write error: %s",
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
		od_error(&instance->log, client->io, "C: write error: %s",
		         machine_error(client->io));
		return -1;
	}
	return 0;
}

enum {
	OD_RS_UNDEF,
	OD_RS_OK,
	OD_RS_EROUTE,
	OD_RS_EPOOL,
	OD_RS_ELIMIT,
	OD_RS_ESERVER_CONFIGURE,
	OD_RS_ESERVER_READ,
	OD_RS_ESERVER_WRITE,
	OD_RS_ECLIENT_READ,
	OD_RS_ECLIENT_WRITE
};

static inline od_routerstatus_t
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
		od_debug(&instance->log, client->io, "C (copy): %c", *stream->s);

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
od_frontend_session(od_client_t *client)
{
	od_instance_t *instance = client->system->instance;

	/* get server connection for the route */
	od_routerstatus_t status;
	status = od_router_attach(client);
	if (status != OD_ROK)
		return OD_RS_EPOOL;

	od_server_t *server;
	server = client->server;

	/* assign client session key */
	server->key_client = client->key;

	/* configure server using client startup parameters */
	int rc;
	rc = od_backend_configure(client->server, &client->startup);
	if (rc == -1)
		return OD_RS_ESERVER_CONFIGURE;

	so_stream_t *stream = &client->stream;
	for (;;)
	{
		/* client to server */
		so_stream_reset(stream);
		rc = od_read(client->io, stream, UINT32_MAX);
		if (rc == -1)
			return OD_RS_ECLIENT_READ;
		int type;
		type = stream->s[rc];
		od_debug(&instance->log, client->io, "C: %c", type);

		/* client graceful shutdown */
		if (type == 'X')
			break;

		rc = od_write(server->io, stream);
		if (rc == -1)
			return OD_RS_ESERVER_WRITE;

		server->count_request++;

		so_stream_reset(stream);
		for (;;) {
			/* pipeline server reply */
			for (;;) {
				rc = od_read(server->io, stream, 1000);
				if (rc >= 0)
					break;
				/* client watchdog.
				 *
				 * ensure that client has not closed
				 * the connection */
				if (! machine_read_timedout(server->io))
					return OD_RS_ESERVER_READ;
				if (machine_connected(client->io))
					continue;
				od_debug(&instance->log, server->io,
				         "S (watchdog): client disconnected");
				return OD_RS_ECLIENT_READ;
			}
			type = stream->s[rc];
			od_debug(&instance->log, server->io, "S: %c", type);

			/* ReadyForQuery */
			if (type == 'Z') {
				rc = od_backend_ready(server, stream->s + rc,
				                      so_stream_used(stream) - rc);
				if (rc == -1)
					return OD_RS_ECLIENT_READ;

				/* flush reply buffer to client */
				rc = od_write(client->io, stream);
				if (rc == -1)
					return OD_RS_ECLIENT_WRITE;

				break;
			}

			/* CopyInResponse */
			if (type == 'G') {
				/* transmit reply to client */
				rc = od_write(client->io, stream);
				if (rc == -1)
					return OD_RS_ECLIENT_WRITE;
				rc = od_frontend_copy_in(client);
				if (rc != OD_RS_OK)
					return rc;
				continue;
			}
			/* CopyOutResponse */
			if (type == 'H') {
				assert(! server->is_copy);
				server->is_copy = 1;
				continue;
			}
			/* copy out complete */
			if (type == 'c') {
				server->is_copy = 0;
				continue;
			}
		}
	}
	return OD_RS_OK;
}

void od_frontend(void *arg)
{
	od_client_t *client = arg;
	od_instance_t *instance = client->system->instance;

	od_log(&instance->log, client->io, "C: new connection");

	/* attach client io to relay machine event loop */
	int rc;
	rc = machine_io_attach(client->io);
	if (rc == -1) {
		od_error(&instance->log, client->io, "failed to transfer client io");
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
		od_debug(&instance->log, client->io, "C: cancel request");

		od_frontend_close(client);
#if 0
		so_key_t key = client->startup.key;
		od_feclose(client);
		od_cancel(pooler, &key);
#endif
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
		od_error(&instance->log, client->io,
		         "C: routing failed, closing");
		od_frontend_close(client);
		return;
	case OD_RERROR_NOT_FOUND:
		od_error(&instance->log, client->io,
		         "C: database route '%s' is not declared, closing",
		         so_parameter_value(client->startup.database));
		od_frontend_close(client);
		return;
	case OD_RERROR_LIMIT:
		od_error(&instance->log, client->io,
		         "C: route connection limit reached, closing");
		od_frontend_close(client);
		return;
	case OD_ROK:;
		od_route_t *route = client->route;
		od_debug(&instance->log, client->io,
		         "C: route to '%s' (using '%s' server)",
		          route->scheme->target,
		          route->scheme->server->name);
		break;
	}

	rc = od_frontend_session(client);

	od_server_t *server;
	server = client->server;
	client->server = NULL;

	switch (rc) {
	case OD_RS_EROUTE:
	case OD_RS_EPOOL:
	case OD_RS_ELIMIT:
		assert(server == NULL);
		break;
	case OD_RS_OK:
	case OD_RS_ECLIENT_READ:
	case OD_RS_ECLIENT_WRITE:
		/* close client connection and reuse server
		 * link in case of client errors and
		 * graceful shutdown */
		if (rc == OD_RS_OK)
			od_log(&instance->log, client->io,
			       "C: disconnected");
		else
			od_log(&instance->log, client->io,
			       "C: disconnected (read/write error): %s",
			       machine_error(client->io));

		rc = od_backend_reset(server);
		if (rc != 1) {
			/* TODO: close backend connection */
			break;
		}

		/* push server to router server pool */
		od_router_detach(server);
		break;
	case OD_RS_ESERVER_CONFIGURE:
		od_log(&instance->log, server->io,
		       "S: disconnected (server configure error): %s",
		       machine_error(server->io));

		/* TODO: close backend connection */
		break;
	case OD_RS_ESERVER_READ:
	case OD_RS_ESERVER_WRITE:
		/* close client connection and close server
		 * connection in case of server errors */
		od_log(&instance->log, server->io,
		       "S: disconnected (read/write error): %s",
		       machine_error(server->io));

		/* TODO: close backend connection */
		break;
	case OD_RS_UNDEF:
		assert(0);
		break;
	}

	/* close frontend connection */
	od_frontend_close(client);
}
