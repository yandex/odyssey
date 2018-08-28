
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
#include <ctype.h>
#include <inttypes.h>
#include <assert.h>

#include <machinarium.h>
#include <kiwi.h>
#include <odyssey.h>

typedef enum {
	OD_FE_UNDEF,
	OD_FE_OK,
	OD_FE_KILL,
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

void
od_frontend_close(od_client_t *client)
{
	assert(client->route == NULL);
	assert(client->server == NULL);
	if (client->io) {
		machine_close(client->io);
		machine_io_free(client->io);
		client->io = NULL;
	}
	if (client->io_notify) {
		machine_close(client->io_notify);
		machine_io_free(client->io_notify);
		client->io_notify = NULL;
	}
	od_client_free(client);
}

int
od_frontend_error(od_client_t *client, char *code, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	machine_msg_t *msg;
	msg = od_frontend_error_msg(client, code, fmt, args);
	va_end(args);
	if (msg == NULL)
		return -1;
	int rc;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	rc = machine_flush(client->io, UINT32_MAX);
	return rc;
}

static int
od_frontend_startup(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;

	machine_msg_t *msg;
	msg = od_read_startup(client->io, UINT32_MAX);
	if (msg == NULL)
		return -1;

	int rc;
	rc = kiwi_be_read_startup(msg, &client->startup);
	if (rc == -1)
		goto error;

	/* client ssl request */
	rc = od_tls_frontend_accept(client, &instance->logger,
	                            client->config_listen,
	                            client->tls);
	if (rc == -1)
		return -1;

	machine_msg_free(msg);
	if (! client->startup.is_ssl_request)
		return 0;

	/* read startup-cancel message followed after ssl
	 * negotiation */
	assert(client->startup.is_ssl_request);
	msg = od_read_startup(client->io, UINT32_MAX);
	if (msg == NULL)
		return -1;
	rc = kiwi_be_read_startup(msg, &client->startup);
	if (rc == -1)
		goto error;

	machine_msg_free(msg);
	return 0;

error:
	machine_msg_free(msg);
	od_error(&instance->logger, "startup", client, NULL,
	         "incorrect startup packet");
	od_frontend_error(client, KIWI_PROTOCOL_VIOLATION,
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

	od_router_status_t status;
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
		rc = od_backend_connect(server, context);
		if (rc == -1)
			return OD_FE_ESERVER_CONNECT;
	}

	if (! od_id_mgr_cmp(&server->last_client_id, &client->id))
	{
		rc = od_deploy_write(client->server, context, &client->params);
		if (rc == -1)
			return OD_FE_ESERVER_WRITE;

		client->server->deploy_sync = rc;

	} else {
		od_debug(&instance->logger, context, client, server,
		         "previously owned, no need to reconfigure %s%.*s",
		         server->id.id_prefix, sizeof(server->id.id),
		         server->id.id);

		client->server->deploy_sync = 0;
	}

	od_server_sync_request(server, server->deploy_sync);
	return OD_FE_OK;
}

static inline int
od_frontend_setup_console(od_client_t *client)
{
	/* console parameters */
	int rc;
	machine_msg_t *msg;
	msg = kiwi_be_write_parameter_status("server_version", 15, "9.6.0", 6);
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	msg = kiwi_be_write_parameter_status("server_encoding", 16, "UTF-8", 6);
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	msg = kiwi_be_write_parameter_status("client_encoding", 16, "UTF-8", 6);
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	msg = kiwi_be_write_parameter_status("DateStyle", 10, "ISO", 4);
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	msg = kiwi_be_write_parameter_status("TimeZone", 9, "GMT", 4);
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	/* ready message */
	msg = kiwi_be_write_ready('I');
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	rc = machine_flush(client->io, UINT32_MAX);
	if (rc == -1)
		return -1;
	return 0;
}

static inline od_frontend_rc_t
od_frontend_setup(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;

	/* copy client startup parameters */
	int rc;
	rc = kiwi_params_copy(&client->params, &client->startup.params);
	if (rc == -1)
		return OD_FE_ECLIENT_CONFIGURE;

	/* attach client to a server and configure it using client
	 * startup parameters */
	od_frontend_rc_t fe_rc;
	fe_rc = od_frontend_attach(client, "setup");
	if (fe_rc != OD_FE_OK)
		return fe_rc;

#if 0
	rc = machine_flush(client->io, UINT32_MAX);
	if (rc == -1)
		return OD_FE_ESERVER_WRITE;
#endif

	/* wait for completion */
	od_server_t *server = client->server;
	rc = od_backend_deploy_wait(server, "setup", UINT32_MAX);
	if (rc == -1)
		return OD_FE_ESERVER_CONFIGURE;

	/* write paremeter status messages */
	od_debug(&instance->logger, "setup", client, server,
	         "sending params:");

	machine_msg_t *msg;
	kiwi_param_t *param = server->params.list;
	while (param)
	{
		msg = kiwi_be_write_parameter_status(kiwi_param_name(param),
		                                     param->name_len,
		                                     kiwi_param_value(param),
		                                     param->value_len);
		if (msg == NULL)
			return OD_FE_ECLIENT_CONFIGURE;

		od_debug(&instance->logger, "setup", client, server,
		         " %.*s = %.*s",
		         param->name_len,
		         kiwi_param_name(param),
		         param->value_len,
		         kiwi_param_value(param));

		rc = machine_write(client->io, msg);
		if (rc == -1)
			return OD_FE_ECLIENT_WRITE;

		param = param->next;
	}

	/* write key data message */
	msg = kiwi_be_write_backend_key_data(client->key.key_pid, client->key.key);
	if (msg == NULL)
		return OD_FE_ECLIENT_CONFIGURE;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return OD_FE_ECLIENT_WRITE;

	/* write ready message */
	msg = kiwi_be_write_ready('I');
	if (msg == NULL)
		return OD_FE_ECLIENT_CONFIGURE;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return OD_FE_ECLIENT_WRITE;

	/* done */
	rc = machine_flush(client->io, UINT32_MAX);
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

static inline od_frontend_rc_t
od_frontend_remote_client(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;
	od_route_t *route = client->route;
	od_server_t *server = client->server;

	/* get server connection from the route pool, write configuration
	 * requests before client request */
	if (server == NULL) {
		od_frontend_rc_t fe_rc;
		fe_rc = od_frontend_attach(client, "main");
		if (fe_rc != OD_FE_OK)
			return fe_rc;
		server = client->server;
		od_server_sync_request(server, server->deploy_sync);
	}

	do
	{
		machine_msg_t *msg;
		msg = od_read(client->io, UINT32_MAX);
		if (msg == NULL)
			return OD_FE_ECLIENT_READ;

		/* update client recv stat */
		od_stat_recv_client(&route->stats, machine_msg_get_size(msg));

		kiwi_fe_type_t type;
		type = *(char*)machine_msg_get_data(msg);

		od_debug(&instance->logger, "main", client, server, "%s",
		         kiwi_fe_type_to_string(type));

		int rc;
		switch (type) {
		case KIWI_FE_TERMINATE:
			machine_msg_free(msg);
			return OD_FE_TERMINATE;

		case KIWI_FE_COPY_DONE:
		case KIWI_FE_COPY_FAIL:
			server->is_copy = 0;
			break;

		case KIWI_FE_QUERY:
			if (instance->config.log_query)
			{
				uint32_t query_len;
				char *query;
				rc = kiwi_be_read_query(msg, &query, &query_len);
				if (rc == -1) {
					od_error(&instance->logger, "main", client, server,
					         "failed to parse %s",
					         kiwi_fe_type_to_string(type));
					break;
				}
				od_log(&instance->logger, "main", client, server,
				       "%.*s", query_len, query);
			}
			break;

		case KIWI_FE_PARSE:
			if (instance->config.log_query)
			{
				uint32_t name_len;
				char *name;
				uint32_t query_len;
				char *query;
				rc = kiwi_be_read_parse(msg, &name, &name_len, &query, &query_len);
				if (rc == -1) {
					od_error(&instance->logger, "main", client, server,
					         "failed to parse %s",
					         kiwi_fe_type_to_string(type));
					break;
				}
				if (! name_len) {
					name = "<unnamed>";
					name_len = 9;
				}
				od_log(&instance->logger, "main", client, server,
				       "prepare %.*s: %.*s", name_len, name, query_len, query);
			}
			break;

		default:
			break;
		}

		/* forward message to server */
		rc = machine_write(server->io, msg);
		if (rc == -1)
			return OD_FE_ESERVER_WRITE;

		if (type == KIWI_FE_QUERY ||
		    type == KIWI_FE_FUNCTION_CALL ||
		    type == KIWI_FE_SYNC)
		{
			/* update server sync state */
			od_server_sync_request(server, 1);
		}

		/* update server stats */
		od_stat_query_start(&server->stats_state);

	} while (machine_read_pending(client->io));

	return OD_FE_OK;
}

static inline od_frontend_rc_t
od_frontend_remote_server(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;
	od_route_t *route = client->route;
	od_server_t *server = client->server;

	do
	{
		machine_msg_t *msg;
		msg = od_read(server->io, UINT32_MAX);
		if (msg == NULL)
			return OD_FE_ESERVER_READ;

		/* update server recv stats */
		od_stat_recv_server(&route->stats, machine_msg_get_size(msg));

		kiwi_be_type_t type;
		type = *(char*)machine_msg_get_data(msg);

		od_debug(&instance->logger, "main", client, server, "%s",
		         kiwi_be_type_to_string(type));

		/* discard replies during configuration deploy */
		int rc;
		if (server->deploy_sync > 0) {
			rc = od_backend_deploy(server, "main-deploy", msg);
			machine_msg_free(msg);
			if (rc == -1)
				return OD_FE_ESERVER_CONFIGURE;
			continue;
		}

		switch (type) {
		case KIWI_BE_ERROR_RESPONSE:
			od_backend_error(server, "main", msg);
			break;
		case KIWI_BE_PARAMETER_STATUS: {
			char *name;
			uint32_t name_len;
			char *value;
			uint32_t value_len;
			rc = kiwi_fe_read_parameter(msg, &name, &name_len, &value, &value_len);
			if (rc == -1) {
				machine_msg_free(msg);
				od_error(&instance->logger, "main", client, server,
				         "failed to parse ParameterStatus message");
				return OD_FE_ESERVER_READ;
			}
			od_debug(&instance->logger, "main", client, server,
			         "%.*s = %.*s",
			         name_len, name, value_len, value);

			/* update server and current client parameter state */
			kiwi_param_t *param;
			param = kiwi_param_allocate(name, name_len, value, value_len);
			if (param == NULL) {
				machine_msg_free(msg);
				return OD_FE_ESERVER_CONFIGURE;
			}
			kiwi_params_replace(&server->params, param);
			param = kiwi_param_allocate(name, name_len, value, value_len);
			if (param == NULL) {
				machine_msg_free(msg);
				return OD_FE_ESERVER_CONFIGURE;
			}
			kiwi_params_replace(&client->params, param);
			break;
		}

		case KIWI_BE_COPY_IN_RESPONSE:
		case KIWI_BE_COPY_OUT_RESPONSE:
			server->is_copy = 1;
			break;
		case KIWI_BE_COPY_DONE:
			server->is_copy = 0;
			break;

		case KIWI_BE_READY_FOR_QUERY:
		{
			rc = od_backend_ready(server, msg);
			if (rc == -1) {
				machine_msg_free(msg);
				return OD_FE_ESERVER_READ;
			}

			/* update server stats */
			int64_t query_time = 0;
			od_stat_query_end(&route->stats, &server->stats_state,
			                  server->is_transaction,
			                  &query_time);
			if (query_time > 0) {
				od_debug(&instance->logger, "main", server->client, server,
				         "query time: %d microseconds",
				         query_time);
			}

			/* handle transaction pooling */
			if (route->config->pool == OD_POOL_TYPE_TRANSACTION) {
				if (! server->is_transaction) {
					rc = machine_flush(server->io, UINT32_MAX);
					if (rc == -1) {
						machine_msg_free(msg);
						return OD_FE_ESERVER_WRITE;
					}
					/* cleanup server */
					rc = od_reset(server);
					if (rc == -1) {
						machine_msg_free(msg);
						return OD_FE_ESERVER_WRITE;
					}
					/* push server connection back to route pool */
					od_router_detach(client);
					server = NULL;
				}
			}
			break;
		}
		default:
			break;
		}

		/* forward message to client */
		rc = machine_write(client->io, msg);
		if (rc == -1)
			return OD_FE_ECLIENT_WRITE;

		if (client->server == NULL)
			break;

	} while (machine_read_pending(server->io));

	return OD_FE_OK;
}

static od_frontend_rc_t
od_frontend_ctl(od_client_t *client)
{
	od_client_notify_read(client);
	if (client->ctl.op == OD_CLIENT_OP_KILL)
		return OD_FE_KILL;
	return OD_FE_OK;
}

static od_frontend_rc_t
od_frontend_remote(od_client_t *client)
{
	machine_io_t *io_ready[3];
	machine_io_t *io_set[3];
	int           io_count = 2;
	int           io_pos;
	io_set[0] = client->io_notify;
	io_set[1] = client->io;
	io_set[2] = NULL;

	for (;;)
	{
		int ready;
		ready = machine_read_poll(io_set, io_ready, io_count, UINT32_MAX);

		for (io_pos = 0; io_pos < ready; io_pos++)
		{
			machine_io_t *io = io_ready[io_pos];
			od_frontend_rc_t fe_rc;
			if (io == client->io_notify) {
				fe_rc = od_frontend_ctl(client);
				if (fe_rc != OD_FE_OK)
					return fe_rc;
				continue;
			}
			if (io == client->io) {
				fe_rc = od_frontend_remote_client(client);
				if (fe_rc != OD_FE_OK)
					return fe_rc;
				assert(client->server != NULL);
				io_count  = 3;
				io_set[2] = client->server->io;
				continue;
			}
			fe_rc = od_frontend_remote_server(client);
			if (fe_rc != OD_FE_OK)
				return fe_rc;
			if (client->server == NULL) {
				io_count  = 2;
				io_set[2] = NULL;
				break;
			}
		}
	}

	/* unreach */
	abort();
	return OD_FE_UNDEF;
}

static od_frontend_rc_t
od_frontend_local(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;

	/* create non-shared channel for result */
	machine_channel_t *channel;
	channel = machine_channel_create(0);
	if (channel == NULL)
		return OD_FE_ECLIENT_READ;

	for (;;)
	{
		/* read client request */
		machine_msg_t *msg;
		msg = od_read(client->io, UINT32_MAX);
		if (msg == NULL) {
			machine_channel_free(channel);
			return OD_FE_ECLIENT_READ;
		}

		kiwi_fe_type_t type = *(char*)machine_msg_get_data(msg);
		od_debug(&instance->logger, "local", client, NULL, "%s",
		         kiwi_fe_type_to_string(type));

		if (type == KIWI_FE_TERMINATE) {
			machine_msg_free(msg);
			break;
		}

		int rc;
		if (type == KIWI_FE_QUERY)
		{
			rc = od_console_request(client, channel, msg);
			machine_msg_free(msg);
			if (rc == -1) {
				machine_channel_free(channel);
				return OD_FE_ECLIENT_WRITE;
			}
			rc = machine_write_batch(client->io, channel);
			if (rc == -1) {
				machine_channel_free(channel);
				return OD_FE_ECLIENT_WRITE;
			}
			continue;
		}

		/* unsupported */
		machine_msg_free(msg);

		od_error(&instance->logger, "local", client, NULL,
		         "unsupported request '%s'",
		         kiwi_fe_type_to_string(type));

		od_frontend_error(client, KIWI_FEATURE_NOT_SUPPORTED,
		                  "unsupported request '%s'",
		                  kiwi_fe_type_to_string(type));

		msg = kiwi_be_write_ready('I');
		if (msg == NULL) {
			machine_channel_free(channel);
			return OD_FE_ECLIENT_WRITE;
		}
		rc = machine_write(client->io, msg);
		if (rc == -1) {
			machine_channel_free(channel);
			return OD_FE_ECLIENT_WRITE;
		}
	}

	machine_channel_free(channel);
	return OD_FE_OK;
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
		od_frontend_error(client, KIWI_CONNECTION_FAILURE,
		                  "failed to get remote server connection");
		/* detach client from route */
		od_unroute(client);
		break;

	case OD_FE_KILL:
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
		od_frontend_error(client, KIWI_CONNECTION_FAILURE,
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
		if (route->config->client_fwd_error) {
			/* todo */
			/* forward server error to client */
			/*od_frontend_error_fwd(client);*/
		} else {
			od_frontend_error(client, KIWI_CONNECTION_FAILURE,
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
		od_frontend_error(client, KIWI_CONNECTION_FAILURE,
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
		od_frontend_error(client, KIWI_CONNECTION_FAILURE,
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

void
od_frontend(void *arg)
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
		machine_close(client->io_notify);
		od_client_free(client);
		return;
	}

	rc = machine_io_attach(client->io_notify);
	if (rc == -1) {
		od_error(&instance->logger, "startup", client, NULL,
		         "failed to transfer client notify io");
		machine_close(client->io);
		machine_close(client->io_notify);
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
		od_log(&instance->logger, "startup", client, NULL,
		       "cancel request");
		od_router_cancel_t cancel;
		od_router_cancel_init(&cancel);
		rc = od_router_cancel(client, &cancel);
		if (rc == 0) {
			od_cancel(client->global, cancel.config, &cancel.key,
			          &cancel.id);
			od_router_cancel_free(&cancel);
		}
		od_frontend_close(client);
		return;
	}

	/* set client backend key */
	od_frontend_key(client);

	/* route client */
	od_router_status_t status;
	status = od_route(client);
	switch (status) {
	case OD_RERROR:
		od_error(&instance->logger, "startup", client, NULL,
		         "routing failed, closing");
		od_frontend_error(client, KIWI_SYSTEM_ERROR,
		                  "client routing failed");
		od_frontend_close(client);
		return;
	case OD_RERROR_NOT_FOUND:
		od_error(&instance->logger, "startup", client, NULL,
		         "route for '%s.%s' is not found, closing",
		         kiwi_param_value(client->startup.database),
		         kiwi_param_value(client->startup.user));
		od_frontend_error(client, KIWI_UNDEFINED_DATABASE,
		                  "route for '%s.%s' is not found",
		                  kiwi_param_value(client->startup.database),
		                  kiwi_param_value(client->startup.user));
		od_frontend_close(client);
		return;
	case OD_RERROR_LIMIT:
		od_error(&instance->logger, "startup", client, NULL,
		         "route connection limit reached, closing");
		od_frontend_error(client, KIWI_TOO_MANY_CONNECTIONS,
		                  "too many connections");
		od_frontend_close(client);
		return;
	case OD_ROK:
	{
		od_route_t *route = client->route;
		if (instance->config.log_session) {
			od_log(&instance->logger, "startup", client, NULL,
			       "route '%s.%s' to '%s.%s'",
			       kiwi_param_value(client->startup.database),
			       kiwi_param_value(client->startup.user),
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
	od_route_t *route = client->route;
	od_frontend_rc_t frontend_rc;
	switch (route->config->storage->storage_type) {
	case OD_STORAGE_TYPE_REMOTE:
		frontend_rc = od_frontend_setup(client);
		break;
	case OD_STORAGE_TYPE_LOCAL:
		frontend_rc = od_frontend_setup_console(client);
		break;
	}
	if (frontend_rc != OD_FE_OK) {
		od_frontend_cleanup(client, "setup", frontend_rc);
		od_frontend_close(client);
		return;
	}

	/* main */
	switch (route->config->storage->storage_type) {
	case OD_STORAGE_TYPE_REMOTE:
		frontend_rc = od_frontend_remote(client);
		break;
	case OD_STORAGE_TYPE_LOCAL:
		frontend_rc = od_frontend_local(client);
		break;
	}

	od_frontend_cleanup(client, "main", frontend_rc);

	/* close frontend connection */
	od_frontend_close(client);
}
