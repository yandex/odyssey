
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
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
#include "sources/config.h"
#include "sources/config_reader.h"
#include "sources/msg.h"
#include "sources/global.h"
#include "sources/server.h"
#include "sources/server_pool.h"
#include "sources/client.h"
#include "sources/client_pool.h"
#include "sources/route_id.h"
#include "sources/route.h"
#include "sources/route_pool.h"
#include "sources/io.h"
#include "sources/instance.h"
#include "sources/router_cancel.h"
#include "sources/router.h"
#include "sources/system.h"
#include "sources/worker.h"
#include "sources/frontend.h"
#include "sources/backend.h"
#include "sources/cancel.h"
#include "sources/reset.h"
#include "sources/deploy.h"
#include "sources/tls.h"
#include "sources/auth.h"
#include "sources/console.h"

typedef enum {
	OD_FE_UNDEF,
	OD_FE_OK,
	OD_FE_TERMINATE,
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
	od_instance_t *instance = client->global->instance;
	assert(client->route == NULL);
	assert(client->server == NULL);
	if (client->stream) {
		od_client_stream_detach(client, &instance->stream_cache);
	}
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
	od_instance_t *instance = client->global->instance;
	od_server_t *server = client->server;
	assert(server != NULL);
	assert(server->stats.count_error != 0);
	(void)server;
	shapito_fe_error_t error;
	int rc;
	rc = shapito_fe_read_error(&error, client->stream->start,
	                           shapito_stream_used(client->stream));
	if (rc == -1)
		return -1;

	shapito_stream_t *stream;
	stream = shapito_cache_pop(&instance->stream_cache);
	shapito_stream_reset(stream);
	char msg[512];
	int  msg_len;
	msg_len = od_snprintf(msg, sizeof(msg), "odyssey: %s%.*s: %s",
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
	if (rc == -1) {
		shapito_cache_push(&instance->stream_cache, stream);
		return -1;
	}
	rc = od_write(client->io, stream);
	shapito_cache_push(&instance->stream_cache, stream);
	return rc;
}

static inline int
od_frontend_verror(od_client_t *client, char *code, char *fmt, va_list args)
{
	char msg[512];
	int  msg_len;
	msg_len = od_snprintf(msg, sizeof(msg), "odyssey: %s%.*s: ",
	                      client->id.id_prefix,
	                      (signed)sizeof(client->id.id),
	                      client->id.id);
	msg_len += od_vsnprintf(msg + msg_len, sizeof(msg) - msg_len, fmt, args);
	shapito_stream_t *stream = client->stream;
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
	shapito_stream_t *stream = client->stream;
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
	od_instance_t *instance = client->global->instance;

	shapito_stream_t *stream = client->stream;
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
	od_instance_t *instance = client->global->instance;

	int rc;
	rc = od_frontend_startup_read(client);
	if (rc == -1)
		return -1;
	shapito_stream_t *stream = client->stream;
	rc = shapito_be_read_startup(&client->startup, stream->start,
	                             shapito_stream_used(stream));
	if (rc == -1)
		goto error;

	/* client ssl request */
	rc = od_tls_frontend_accept(client, &instance->logger,
	                            client->config_listen,
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
	client->key.key_pid = client->id.id_a;
	client->key.key     = client->id.id_b;
}

static inline od_frontend_rc_t
od_frontend_attach(od_client_t *client, char *context)
{
	od_instance_t *instance = client->global->instance;

	od_routerstatus_t status;
	od_server_t *server;

	for (;;)
	{
		status = od_router_attach(client);
		if (status != OD_ROK)
			return OD_FE_EATTACH;
		server = client->server;

		if (server->io && !machine_connected(server->io)) {
			od_log(&instance->logger, context, client, server,
			       "server disconnected, close connection and retry attach");
			od_router_close(client);
			server = NULL;
			continue;
		}

		od_debug(&instance->logger, context, client, server,
		         "attached to %s%.*s",
		         server->id.id_prefix, sizeof(server->id.id),
		         server->id.id);
		break;
	}

	/* connect to server, if necessary */
	int rc;
	if (server->io == NULL) {
		rc = od_backend_connect(server, client->stream, context);
		if (rc == -1)
			return OD_FE_ESERVER_CONNECT;
	}

	shapito_stream_reset(client->stream);

	if (! od_idmgr_cmp(&server->last_client_id, &client->id))
	{
		rc = od_deploy_write(client->server, context, client->stream,
		                     &client->params);
		if (rc == -1) {
			status = od_router_close(client);
			if (status != OD_ROK)
				return OD_FE_EATTACH;
		}

		client->server->deploy_sync = rc;

	} else {
		od_debug(&instance->logger, context, client, server,
		         "previously owned, no need to reconfigure %s%.*s",
		         server->id.id_prefix, sizeof(server->id.id),
		         server->id.id);

		client->server->deploy_sync = 0;
	}

	return OD_FE_OK;
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
	od_instance_t *instance = client->global->instance;
	od_route_t *route = client->route;
	shapito_stream_t *stream = client->stream;
	shapito_stream_reset(stream);

	/* configure console client */
	int rc;
	if (route->config->storage->storage_type == OD_STORAGETYPE_LOCAL) {
		rc = od_frontend_setup_console(stream);
		if (rc == -1)
			return OD_FE_EATTACH;
		rc = od_write(client->io, stream);
		if (rc == -1)
			return OD_FE_ECLIENT_WRITE;
		return OD_FE_OK;
	}

	/* copy client startup parameters */
	rc = shapito_parameters_copy(&client->params, &client->startup.params);
	if (rc == -1)
		return OD_FE_ECLIENT_CONFIGURE;

	/* attach client to a server and configure it using client
	 * startup parameters */
	od_frontend_rc_t fe_rc;
	fe_rc = od_frontend_attach(client, "setup");
	if (fe_rc != OD_FE_OK)
		return fe_rc;
	od_server_t *server;
	server = client->server;

	/* send configuration to the server */
	rc = od_write(server->io, stream);
	if (rc == -1)
		return OD_FE_ESERVER_WRITE;

	od_server_stat_request(server, server->deploy_sync);

	/* wait for completion */
	rc = od_backend_deploy_wait(server, client->stream, "setup", UINT32_MAX);
	if (rc == -1)
		return OD_FE_ESERVER_CONFIGURE;

	shapito_stream_reset(client->stream);

	/* append paremeter status messages */
	od_debug(&instance->logger, "setup", client, server,
	         "sending params:");

	shapito_parameter_t *param;
	shapito_parameter_t *end;
	param = (shapito_parameter_t*)server->params.buf.start;
	end = (shapito_parameter_t*)server->params.buf.pos;
	while (param < end) {
		rc = shapito_be_write_parameter_status(stream,
		                                       shapito_parameter_name(param),
		                                       param->name_len,
		                                       shapito_parameter_value(param),
		                                       param->value_len);
		if (rc == -1)
			return OD_FE_ECLIENT_CONFIGURE;
		od_debug(&instance->logger, "setup", client, server,
		         " %.*s = %.*s",
		         param->name_len,
		         shapito_parameter_name(param),
		         param->value_len,
		         shapito_parameter_value(param));
		param = shapito_parameter_next(param);
	}

	/* append key data message */
	rc = shapito_be_write_backend_key_data(stream, client->key.key_pid,
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

	client->time_setup = machine_time();

	if (instance->config.log_session) {
		od_log(&instance->logger, "setup", client, NULL,
		       "login time: %d microseconds",
		       (client->time_setup - client->time_accept));
	}

	/* put server back to route queue */
	od_router_detach(client);
	return OD_FE_OK;
}

static inline int
od_frontend_stream_hit_limit(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;
	return shapito_stream_used(client->stream) >= instance->config.pipeline;
}

static od_frontend_rc_t
od_frontend_local(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;
	int rc;

	shapito_stream_t *stream = client->stream;
	for (;;)
	{
		/* read client request */
		shapito_stream_reset(stream);
		rc = od_read(client->io, stream, UINT32_MAX);
		if (rc == -1)
			return OD_FE_ECLIENT_READ;

		shapito_fe_msg_t type = *stream->start;
		od_debug(&instance->logger, "local", client, NULL, "%s",
		         shapito_fe_msg_to_string(type));

		if (type == SHAPITO_FE_TERMINATE)
			break;

		if (type == SHAPITO_FE_QUERY) {
			od_consolestatus_t cs;
			cs = od_console_request(client, stream->start,
			                        shapito_stream_used(stream));
			if (cs == OD_CERROR) {
			}
			rc = od_write(client->io, stream);
			if (rc == -1)
				return OD_FE_ECLIENT_WRITE;
			continue;
		}

		/* unsupported */
		od_error(&instance->logger, "local", client, NULL,
		         "unsupported request '%s'",
		         shapito_fe_msg_to_string(type));
		od_frontend_error(client, SHAPITO_FEATURE_NOT_SUPPORTED,
		                  "unsupported request '%s'",
		                  shapito_fe_msg_to_string(type));
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

static inline od_frontend_rc_t
od_frontend_remote_client(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;
	od_server_t *server = client->server;
	shapito_stream_t *stream = client->stream;
	shapito_stream_reset(stream);

	int request_count = 0;
	int terminate = 0;

	/* get server connection from the route pool, write configuration
	 * requests before client request */
	if (server == NULL) {
		od_frontend_rc_t fe_rc;
		fe_rc = od_frontend_attach(client, "main");
		if (fe_rc != OD_FE_OK)
			return fe_rc;
		server = client->server;
		request_count = server->deploy_sync;
	}

	int rc;
	for (;;)
	{
		int request_start = shapito_stream_used(stream);
		rc = od_read(client->io, stream, UINT32_MAX);
		if (rc == -1)
			return OD_FE_ECLIENT_READ;
		char *request = stream->start + request_start;
		int   request_size = shapito_stream_used(stream) - request_start;

		/* update client recv stat */
		od_server_stat_recv_client(server, request_size);

		shapito_fe_msg_t type = *request;
		od_debug(&instance->logger, "main", client, server, "%s",
		         shapito_fe_msg_to_string(type));

		if (type == SHAPITO_FE_TERMINATE) {
			/* discard terminate request */
			stream->pos = stream->start + request_start;
			terminate = 1;
			if (request_count == server->deploy_sync) {
				shapito_stream_reset(stream);
				server->deploy_sync = 0;
			}
			break;
		}

		switch (type) {
		case SHAPITO_FE_COPY_DONE:
		case SHAPITO_FE_COPY_FAIL:
			server->is_copy = 0;
			break;
		case SHAPITO_FE_QUERY:
			if (instance->config.log_query) {
				uint32_t query_len;
				char *query;
				rc = shapito_be_read_query(&query, &query_len, request, request_size);
				if (rc == 0) {
					od_log(&instance->logger, "main", client, server,
					       "%.*s", query_len, query);
				} else {
					od_error(&instance->logger, "main", client, server,
					         "%s", "failed to parse Query");
				}
			}
			break;
		case SHAPITO_FE_PARSE:
			if (instance->config.log_query) {
				char *name, *query;
				rc = shapito_be_read_parse(&name, &query, request, request_size);
				if (rc == 0) {
					od_log(&instance->logger, "main", client, server,
					       "prepare %s: %s",
						   name[0]=='\0'?"<unnamed>":name,
						   query);
				} else {
					od_error(&instance->logger, "main", client, server,
					         "%s", "failed to parse Parse");
				}
			}
			break;
		default:
			break;
		}

		if (type == SHAPITO_FE_QUERY ||
		    type == SHAPITO_FE_FUNCTION_CALL ||
		    type == SHAPITO_FE_SYNC)
		{
			request_count++;
		}

		rc = od_frontend_stream_hit_limit(client);
		if (rc)
			break;

		rc = machine_read_pending(client->io);
		if (rc < 0 || rc > 0)
			continue;
		break;
	}

	/* forward to server */
	if (shapito_stream_used(stream) > 0)
	{
		rc = od_write(server->io, stream);
		if (rc == -1)
			return OD_FE_ESERVER_WRITE;

		/* update server sync state */
		od_server_stat_request(server, request_count);
	}

	if (terminate)
		return OD_FE_TERMINATE;

	return OD_FE_OK;
}

static inline od_frontend_rc_t
od_frontend_remote_server(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;
	od_route_t *route = client->route;
	od_server_t *server = client->server;
	shapito_stream_t *stream = client->stream;
	shapito_stream_reset(stream);

	int rc;
	for (;;)
	{
		int request_start = shapito_stream_used(stream);
		rc = od_read(server->io, stream, UINT32_MAX);
		if (rc == -1)
			return OD_FE_ESERVER_READ;
		char *request = stream->start + request_start;
		int   request_size = shapito_stream_used(stream) - request_start;

		/* update server recv stats */
		od_server_stat_recv_server(server, request_size);

		shapito_be_msg_t type = *request;
		od_debug(&instance->logger, "main", client, server, "%s",
		         shapito_be_msg_to_string(type));

		/* discard replies during configuration deploy */
		if (server->deploy_sync > 0) {
			rc = od_backend_deploy(server, "main-deploy", request, request_size);
			if (rc == -1)
				return OD_FE_ESERVER_CONFIGURE;
			shapito_stream_reset(stream);
			continue;
		}

		if (type == SHAPITO_BE_READY_FOR_QUERY) {
			rc = od_backend_ready(server, "main", request, request_size);
			if (rc == -1)
				return OD_FE_ECLIENT_READ;

			/* handle transaction pooling */
			if (route->config->pool == OD_POOLING_TRANSACTION) {
				if (! server->is_transaction) {
					/* cleanup server */
					rc = od_reset(server);
					if (rc == -1)
						return OD_FE_ESERVER_WRITE;
					/* push server connection back to route pool */
					od_router_detach(client);
					server = NULL;
				}
				break;
			}
		}

		switch (type) {
		case SHAPITO_BE_ERROR_RESPONSE:
			od_backend_error(server, "main", request, request_size);
			break;
		case SHAPITO_BE_PARAMETER_STATUS: {
			char *name;
			uint32_t name_len;
			char *value;
			uint32_t value_len;
			rc = shapito_fe_read_parameter(request, request_size, &name, &name_len,
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
			break;
		}

		case SHAPITO_BE_COPY_IN_RESPONSE:
		case SHAPITO_BE_COPY_OUT_RESPONSE:
			server->is_copy = 1;
			break;
		case SHAPITO_BE_COPY_DONE:
			server->is_copy = 0;
			break;
		default:
			break;
		}

		rc = od_frontend_stream_hit_limit(client);
		if (rc)
			break;

		rc = machine_read_pending(server->io);
		if (rc < 0 || rc > 0)
			continue;
		break;
	}

	/* forward to client */
	if (shapito_stream_used(stream) > 0) {
		rc = od_write(client->io, stream);
		if (rc == -1)
			return OD_FE_ECLIENT_WRITE;
	}

	return OD_FE_OK;
}

static od_frontend_rc_t
od_frontend_remote(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;
	assert(client->stream != NULL);

	machine_io_t *io_ready[2];
	machine_io_t *io_set[2];
	int           io_count = 1;
	int           io_pos;
	io_set[0] = client->io;
	io_set[1] = NULL;

	for (;;)
	{
		od_client_stream_detach(client, &instance->stream_cache);

		int ready;
		ready = machine_read_poll(io_set, io_ready, io_count, UINT32_MAX);

		od_client_stream_attach(client, &instance->stream_cache);

		for (io_pos = 0; io_pos < ready; io_pos++)
		{
			machine_io_t *io = io_ready[io_pos];
			od_frontend_rc_t fe_rc;
			if (io == client->io) {
				fe_rc = od_frontend_remote_client(client);
				if (fe_rc != OD_FE_OK)
					return fe_rc;
				assert(client->server != NULL);
				io_count  = 2;
				io_set[1] = client->server->io;
				continue;
			}
			fe_rc = od_frontend_remote_server(client);
			if (fe_rc != OD_FE_OK)
				return fe_rc;
			if (client->server == NULL) {
				io_count  = 1;
				io_set[1] = NULL;
				break;
			}
		}
	}

	/* unreach */
	abort();
	return OD_FE_UNDEF;
}

static void
od_frontend_cleanup(od_client_t *client, char *context,
                    od_frontend_rc_t status)
{
	od_instance_t *instance = client->global->instance;
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

	case OD_FE_TERMINATE:
	case OD_FE_OK:
		/* graceful disconnect */
		if (instance->config.log_session) {
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
		if (route->config->client_fwd_error && server->stats.count_error) {
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
	od_instance_t *instance = client->global->instance;

	/* log client connection */
	if (instance->config.log_session) {
		char peer[128];
		od_getpeername(client->io, peer, sizeof(peer), 1, 1);
		od_log(&instance->logger, "startup", client, NULL,
		       "new client connection %s",
		       peer);
	}

	/* attach client io to worker machine event loop */
	int rc;
	rc = machine_io_attach(client->io);
	if (rc == -1) {
		od_error(&instance->logger, "startup", client, NULL,
		         "failed to transfer client io");
		machine_close(client->io);
		od_client_free(client);
		return;
	}

	/* attach stream to the client */
	od_client_stream_attach(client, &instance->stream_cache);

	/* handle startup */
	rc = od_frontend_startup(client);
	if (rc == -1) {
		od_frontend_close(client);
		return;
	}

	/* handle cancel request */
	if (client->startup.is_cancel) {
		od_log(&instance->logger, "startup", client, NULL,
		       "cancel request");
		od_routercancel_t cancel;
		od_routercancel_init(&cancel);
		rc = od_router_cancel(client, &cancel);
		if (rc == 0) {
			od_cancel(client->global, client->stream, cancel.config,
			          &cancel.key, &cancel.id);
			od_routercancel_free(&cancel);
		}
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
		if (instance->config.log_session) {
			od_log(&instance->logger, "startup", client, NULL,
			       "route '%s.%s' to '%s.%s'",
			       shapito_parameter_value(client->startup.database),
			       shapito_parameter_value(client->startup.user),
			       route->config->db_name,
			       route->config->user_name);
		}
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
	switch (route->config->storage->storage_type) {
	case OD_STORAGETYPE_REMOTE:
		frontend_rc = od_frontend_remote(client);
		break;
	case OD_STORAGETYPE_LOCAL:
		frontend_rc = od_frontend_local(client);
		break;
	}

	/* reset client and server state */
	if (client->stream == NULL)
		od_client_stream_attach(client, &instance->stream_cache);

	od_frontend_cleanup(client, "main", frontend_rc);

	/* close frontend connection */
	od_frontend_close(client);
}
