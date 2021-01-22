
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

static inline void od_frontend_close(od_client_t *client)
{
	assert(client->route == NULL);
	assert(client->server == NULL);

	od_router_t *router = client->global->router;
	od_atomic_u32_dec(&router->clients);

	od_io_close(&client->io);
	if (client->notify_io) {
		machine_close(client->notify_io);
		machine_io_free(client->notify_io);
		client->notify_io = NULL;
	}
	od_client_free(client);
}

int od_frontend_error(od_client_t *client, char *code, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	machine_msg_t *msg;
	msg = od_frontend_error_msg(client, NULL, code, fmt, args);
	va_end(args);
	if (msg == NULL)
		return -1;
	return od_write(&client->io, msg);
}

static inline int od_frontend_error_fwd(od_client_t *client)
{
	od_server_t *server = client->server;
	assert(server != NULL);
	assert(server->error_connect != NULL);
	kiwi_fe_error_t error;
	int rc;
	rc = kiwi_fe_read_error(machine_msg_data(server->error_connect),
				machine_msg_size(server->error_connect),
				&error);
	if (rc == -1)
		return -1;
	char text[512];
	int text_len;
	text_len =
		od_snprintf(text, sizeof(text), "odyssey: %s%.*s: %s",
			    client->id.id_prefix, (signed)sizeof(client->id.id),
			    client->id.id, error.message);
	int detail_len = error.detail ? strlen(error.detail) : 0;
	int hint_len = error.hint ? strlen(error.hint) : 0;

	machine_msg_t *msg;
	msg = kiwi_be_write_error_as(NULL, error.severity, error.code,
				     error.detail, detail_len, error.hint,
				     hint_len, text, text_len);
	if (msg == NULL)
		return -1;
	return od_write(&client->io, msg);
}

static inline bool
od_frontend_error_is_too_many_connections(od_client_t *client)
{
	od_server_t *server = client->server;
	assert(server != NULL);
	if (server->error_connect == NULL)
		return false;
	kiwi_fe_error_t error;
	int rc;
	rc = kiwi_fe_read_error(machine_msg_data(server->error_connect),
				machine_msg_size(server->error_connect),
				&error);
	if (rc == -1)
		return false;
	return strcmp(error.code, KIWI_TOO_MANY_CONNECTIONS) == 0;
}

static int od_frontend_startup(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;
	machine_msg_t *msg;

	for (int startup_attempt = 0; startup_attempt < MAX_STARTUP_ATTEMPTS;
	     startup_attempt++) {
		msg = od_read_startup(
			&client->io,
			client->config_listen->client_login_timeout);
		if (msg == NULL)
			goto error;

		int rc = kiwi_be_read_startup(machine_msg_data(msg),
					      machine_msg_size(msg),
					      &client->startup, &client->vars);
		machine_msg_free(msg);
		if (rc == -1)
			goto error;

		if (!client->startup.unsupported_request)
			break;
		/* not supported 'N' */
		msg = machine_msg_create(sizeof(uint8_t));
		if (msg == NULL)
			return -1;
		uint8_t *type = machine_msg_data(msg);
		*type = 'N';
		rc = od_write(&client->io, msg);
		if (rc == -1) {
			od_error(&instance->logger,
				 "unsupported protocol (gssapi)", client, NULL,
				 "write error: %s", od_io_error(&client->io));
			return -1;
		}
		od_debug(&instance->logger, "unsupported protocol (gssapi)",
			 client, NULL, "ignoring");
	}

	/* client ssl request */
	int rc = od_tls_frontend_accept(client, &instance->logger,
					client->config_listen, client->tls);
	if (rc == -1)
		return -1;

	if (!client->startup.is_ssl_request) {
		rc = od_compression_frontend_setup(
			client, client->config_listen, &instance->logger);
		if (rc == -1)
			return -1;
		return 0;
	}

	/* read startup-cancel message followed after ssl
	 * negotiation */
	assert(client->startup.is_ssl_request);
	msg = od_read_startup(&client->io,
			      client->config_listen->client_login_timeout);
	if (msg == NULL)
		return -1;
	rc = kiwi_be_read_startup(machine_msg_data(msg), machine_msg_size(msg),
				  &client->startup, &client->vars);
	machine_msg_free(msg);
	if (rc == -1)
		goto error;

	rc = od_compression_frontend_setup(client, client->config_listen,
					   &instance->logger);
	if (rc == -1) {
		return -1;
	}

	return 0;

error:
	od_debug(&instance->logger, "startup", client, NULL,
		 "startup packet read error");
	od_cron_t *cron = client->global->cron;
	od_atomic_u64_inc(&cron->startup_errors);
	return -1;
}

static inline od_frontend_status_t
od_frontend_attach(od_client_t *client, char *context,
		   kiwi_params_t *route_params)
{
	od_instance_t *instance = client->global->instance;
	od_router_t *router = client->global->router;
	od_route_t *route = client->route;

	bool wait_for_idle = false;
	for (;;) {
		od_router_status_t status;
		status = od_router_attach(router, client, wait_for_idle);
		if (status != OD_ROUTER_OK) {
			if (status == OD_ROUTER_ERROR_TIMEDOUT) {
				od_error(&instance->logger, "router", client,
					 NULL,
					 "server pool wait timed out, closing");
				return OD_EATTACH_TOO_MANY_CONNECTIONS;
			}
			return OD_EATTACH;
		}

		od_server_t *server = client->server;
		if (server->io.io && !machine_connected(server->io.io)) {
			od_log(&instance->logger, context, client, server,
			       "server disconnected, close connection and retry attach");
			od_router_close(router, client);
			continue;
		}
		od_debug(&instance->logger, context, client, server,
			 "client %s attached to %s%.*s", client->id.id,
			 server->id.id_prefix, (int)sizeof(server->id.id),
			 server->id.id);

		/* connect to server, if necessary */
		if (server->io.io) {
			return OD_OK;
		}

		int rc;
		od_atomic_u32_inc(&router->servers_routing);
		rc = od_backend_connect(server, context, route_params);
		od_atomic_u32_dec(&router->servers_routing);
		if (rc == -1) {
			/* In case of 'too many connections' error, retry attach attempt by
			 * waiting for a idle server connection for pool_timeout ms
			 */
			wait_for_idle =
				route->rule->pool_timeout > 0 &&
				od_frontend_error_is_too_many_connections(
					client);
			if (wait_for_idle) {
				od_router_close(router, client);
				if (instance->config.server_login_retry) {
					machine_sleep(
						instance->config
							.server_login_retry);
				}
				continue;
			}
			return OD_ESERVER_CONNECT;
		}

		return OD_OK;
	}
}

static inline od_frontend_status_t
od_frontend_attach_and_deploy(od_client_t *client, char *context)
{
	/* attach and maybe connect server */
	od_frontend_status_t status;
	status = od_frontend_attach(client, context, NULL);
	if (status != OD_OK)
		return status;
	od_server_t *server = client->server;

	/* configure server using client parameters */
	int rc;
	rc = od_deploy(client, context);
	if (rc == -1)
		return OD_ESERVER_WRITE;

	/* set number of replies to discard */
	client->server->deploy_sync = rc;

	od_server_sync_request(server, server->deploy_sync);
	return OD_OK;
}

static inline od_frontend_status_t od_frontend_setup_params(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;
	od_router_t *router = client->global->router;
	od_route_t *route = client->route;

	/* ensure route has cached server parameters */
	int rc;
	rc = kiwi_params_lock_count(&route->params);
	if (rc == 0) {
		kiwi_params_t route_params;
		kiwi_params_init(&route_params);

		od_frontend_status_t status;
		status = od_frontend_attach(client, "setup", &route_params);
		if (status != OD_OK) {
			kiwi_params_free(&route_params);
			return status;
		}
		od_router_close(router, client);

		/* There is possible race here, so we will discard our
		 * attempt if params are already set */
		rc = kiwi_params_lock_set_once(&route->params, &route_params);
		if (!rc)
			kiwi_params_free(&route_params);
	}

	od_debug(&instance->logger, "setup", client, NULL, "sending params:");

	/* send parameters set by client or cached by the route */
	kiwi_param_t *param = route->params.params.list;

	machine_msg_t *stream = machine_msg_create(0);
	if (stream == NULL)
		return OD_EOOM;

	while (param) {
		kiwi_var_type_t type;
		type = kiwi_vars_find(&client->vars, kiwi_param_name(param),
				      param->name_len);
		kiwi_var_t *var;
		var = kiwi_vars_get(&client->vars, type);

		machine_msg_t *msg;
		if (var) {
			msg = kiwi_be_write_parameter_status(stream, var->name,
							     var->name_len,
							     var->value,
							     var->value_len);

			od_debug(&instance->logger, "setup", client, NULL,
				 " %.*s = %.*s", var->name_len, var->name,
				 var->value_len, var->value);
		} else {
			msg = kiwi_be_write_parameter_status(
				stream, kiwi_param_name(param), param->name_len,
				kiwi_param_value(param), param->value_len);

			od_debug(&instance->logger, "setup", client, NULL,
				 " %.*s = %.*s", param->name_len,
				 kiwi_param_name(param), param->value_len,
				 kiwi_param_value(param));
		}
		if (msg == NULL) {
			machine_msg_free(stream);
			return OD_EOOM;
		}

		param = param->next;
	}

	rc = od_write(&client->io, stream);
	if (rc == -1)
		return OD_ECLIENT_WRITE;

	return OD_OK;
}

static inline od_frontend_status_t od_frontend_setup(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;

	/* set paremeters */
	od_frontend_status_t status;
	status = od_frontend_setup_params(client);
	if (status != OD_OK)
		return status;

	/* write key data message */
	machine_msg_t *stream;
	machine_msg_t *msg;
	msg = kiwi_be_write_backend_key_data(NULL, client->key.key_pid,
					     client->key.key);
	if (msg == NULL)
		return OD_EOOM;
	stream = msg;

	/* write ready message */
	msg = kiwi_be_write_ready(stream, 'I');
	if (msg == NULL) {
		machine_msg_free(stream);
		return OD_EOOM;
	}

	int rc;
	rc = od_write(&client->io, stream);
	if (rc == -1)
		return OD_ECLIENT_WRITE;

	if (instance->config.log_session) {
		client->time_setup = machine_time_us();
		od_log(&instance->logger, "setup", client, NULL,
		       "login time: %d microseconds",
		       (client->time_setup - client->time_accept));
	}

	return OD_OK;
}

static inline od_frontend_status_t od_frontend_local_setup(od_client_t *client)
{
	machine_msg_t *stream;
	stream = machine_msg_create(0);

	if (stream == NULL)
		goto error;
	/* client parameters */
	machine_msg_t *msg;
	msg = kiwi_be_write_parameter_status(stream, "server_version", 15,
					     "9.6.0", 6);
	if (msg == NULL)
		goto error;
	msg = kiwi_be_write_parameter_status(stream, "server_encoding", 16,
					     "UTF-8", 6);
	if (msg == NULL)
		goto error;
	msg = kiwi_be_write_parameter_status(stream, "client_encoding", 16,
					     "UTF-8", 6);
	if (msg == NULL)
		goto error;
	msg = kiwi_be_write_parameter_status(stream, "DateStyle", 10, "ISO", 4);
	if (msg == NULL)
		goto error;
	msg = kiwi_be_write_parameter_status(stream, "TimeZone", 9, "GMT", 4);
	if (msg == NULL)
		goto error;
	/* ready message */
	msg = kiwi_be_write_ready(stream, 'I');
	if (msg == NULL)
		goto error;
	int rc;
	rc = od_write(&client->io, stream);
	if (rc == -1)
		return OD_ECLIENT_WRITE;
	return OD_OK;
error:
	if (stream)
		machine_msg_free(stream);
	return OD_EOOM;
}

static inline bool od_eject_conn_with_rate(od_client_t *client,
					  od_server_t *server,
					  od_instance_t *instance)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	bool res = false;
        
        if (server == NULL) {
                /* server is null - client was never attached to any server so its not ok to eject this conn  */
                return false;
        }
	od_conn_eject_info *info = server->info;

	pthread_mutex_lock(&info->mu);
	{
		if (info->last_conn_drop_ts + /* 1 sec */ 1 > tv.tv_sec) {
			od_log(&instance->logger, "shutdown", client, server,
			       "delay drop client connection on restart, last drop was too recent (sid %d, last drop %d, curr time %d)",
			       server->id, info->last_conn_drop_ts, tv.tv_sec);
		} else {
			info->last_conn_drop_ts = tv.tv_sec;
			res = true;

			od_log(&instance->logger, "shutdown", client, server,
			       "drop client connection on restart (sid %d, last eject %d, curr time %d)",
			       server->id, info->last_conn_drop_ts, tv.tv_sec);
		}
	}
	pthread_mutex_unlock(&info->mu);

	return res;
}

static inline bool od_should_drop_connection(od_client_t *client,
					     od_server_t *server)
{
	od_instance_t *instance = client->global->instance;

	//TODO:: drop no more than X connection per sec/min/whatever
	if (od_likely(instance->shutdown_worker_id == INVALID_ID)) {
		// try to optimize likely path
		return false;
	}

	if (od_unlikely(client->rule->storage->storage_type ==
			OD_RULE_STORAGE_LOCAL)) {
		/* local server is not very important (db like console, pgbouncer used for stats)*/
		return true;
	}

	switch (client->rule->pool) {
	case OD_RULE_POOL_SESSION: {
		if (od_unlikely(server == NULL)) {
			return od_eject_conn_with_rate(client, server, instance);
		}
		/* TODO: something like drop rate here  */
		if (od_unlikely(!server->is_transaction)) {
			return od_eject_conn_with_rate(client, server, instance);
		}
		return false;
	} break;
	case OD_RULE_POOL_TRANSACTION: {
		if (server == NULL) {
			return od_eject_conn_with_rate(client, server, instance);
		}
		if (od_unlikely(!server->is_transaction)) {
			return od_eject_conn_with_rate(client, server, instance);
		}
		return false;
	} break;
	default:
		return false;
	}
}

static od_frontend_status_t od_frontend_local(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;

	for (;;) {
		machine_msg_t *msg;
		for (;;) {
			/* local server is alwys null */
			if (od_should_drop_connection(client, NULL)) {
				/* Odyssey is in a state of completion, we done 
                         * the last client's request and now we can drop the connection  */

				/* a sort of EAGAIN */
				return OD_ECLIENT_READ;
			}
			/* one minute */
			msg = od_read(&client->io, 60000);

			if (machine_timedout()) {
				/* retry wait to recheck exit condition */
				continue;
			}

			if (msg == NULL) {
				return OD_ECLIENT_READ;
			} else {
				break;
			}
		}

		kiwi_fe_type_t type;
		type = *(char *)machine_msg_data(msg);

		od_debug(&instance->logger, "local", client, NULL, "%s",
			 kiwi_fe_type_to_string(type));

		if (type == KIWI_FE_TERMINATE) {
			machine_msg_free(msg);
			break;
		}

		machine_msg_t *stream = machine_msg_create(0);
		if (stream == NULL) {
			machine_msg_free(msg);
			return OD_EOOM;
		}

		int rc;
		if (type == KIWI_FE_QUERY) {
			rc = od_console_query(client, stream,
					      machine_msg_data(msg),
					      machine_msg_size(msg));
			machine_msg_free(msg);
			if (rc == -1) {
				machine_msg_free(stream);
				return OD_EOOM;
			}
		} else {
			/* unsupported */
			machine_msg_free(msg);

			od_error(&instance->logger, "local", client, NULL,
				 "unsupported request '%s'",
				 kiwi_fe_type_to_string(type));

			msg = od_frontend_errorf(client, stream,
						 KIWI_FEATURE_NOT_SUPPORTED,
						 "unsupported request '%s'",
						 kiwi_fe_type_to_string(type));
			if (msg == NULL) {
				machine_msg_free(stream);
				return OD_EOOM;
			}
		}

		/* ready */
		msg = kiwi_be_write_ready(stream, 'I');
		if (msg == NULL) {
			machine_msg_free(stream);
			return OD_EOOM;
		}

		rc = od_write(&client->io, stream);
		if (rc == -1) {
			return OD_ECLIENT_WRITE;
		}
	}

	return OD_OK;
}

static od_frontend_status_t od_frontend_remote_server(od_relay_t *relay,
						      char *data, int size)
{
	od_client_t *client = relay->on_packet_arg;
	od_server_t *server = client->server;
	od_route_t *route = client->route;
	od_instance_t *instance = client->global->instance;

	kiwi_be_type_t type = *data;
	if (instance->config.log_debug)
		od_debug(&instance->logger, "main", client, server, "%s",
			 kiwi_be_type_to_string(type));

	int is_deploy = od_server_in_deploy(server);
	int is_ready_for_query = 0;

	int rc;
	switch (type) {
	case KIWI_BE_ERROR_RESPONSE:
		od_backend_error(server, "main", data, size);
		break;
	case KIWI_BE_PARAMETER_STATUS:
		rc = od_backend_update_parameter(server, "main", data, size, 0);
		if (rc == -1)
			return relay->error_read;
		break;
	case KIWI_BE_COPY_IN_RESPONSE:
	case KIWI_BE_COPY_OUT_RESPONSE:
		server->is_copy = 1;
		break;
	case KIWI_BE_COPY_DONE:
		server->is_copy = 0;
		break;
	case KIWI_BE_READY_FOR_QUERY: {
		is_ready_for_query = 1;
		od_backend_ready(server, data, size);

		/* update server stats */
		int64_t query_time = 0;
		od_stat_query_end(&route->stats, &server->stats_state,
				  server->is_transaction, &query_time);
		if (instance->config.log_debug && query_time > 0) {
			od_debug(&instance->logger, "main", server->client,
				 server, "query time: %" PRIi64 " microseconds",
				 query_time);
		}

		if (is_deploy)
			server->deploy_sync--;

		break;
	}
	default:
		break;
	}

	/* discard replies during configuration deploy */
	if (is_deploy)
		return OD_SKIP;

	/* handle transaction pooling */
	if (is_ready_for_query) {
		if (route->rule->pool == OD_RULE_POOL_TRANSACTION &&
		    !server->is_transaction && !route->id.physical_rep &&
		    !route->id.logical_rep) {
			return OD_DETACH;
		}
	}

	return OD_OK;
}

static void od_frontend_log_query(od_instance_t *instance, od_client_t *client,
				  char *data, int size)
{
	uint32_t query_len;
	char *query;
	int rc;
	rc = kiwi_be_read_query(data, size, &query, &query_len);
	if (rc == -1)
		return;

	od_log(&instance->logger, "query", client, NULL, "%.*s", query_len,
	       query);
}

static od_frontend_status_t od_frontend_remote_client(od_relay_t *relay,
						      char *data, int size)
{
	od_client_t *client = relay->on_packet_arg;
	od_instance_t *instance = client->global->instance;
	(void)size;

	kiwi_fe_type_t type = *data;
	if (type == KIWI_FE_TERMINATE)
		return OD_STOP;

	/* get server connection from the route pool and write
	   configuration */
	od_server_t *server = client->server;
	assert(server != NULL);

	if (instance->config.log_debug)
		od_debug(&instance->logger, "main", client, server, "%s",
			 kiwi_fe_type_to_string(type));

	switch (type) {
	case KIWI_FE_COPY_DONE:
	case KIWI_FE_COPY_FAIL:
		server->is_copy = 0;
		break;
	case KIWI_FE_QUERY:
		if (instance->config.log_query)
			od_frontend_log_query(instance, client, data, size);
		/* update server sync state */
		od_server_sync_request(server, 1);
		break;
	case KIWI_FE_FUNCTION_CALL:
	case KIWI_FE_SYNC:
		/* update server sync state */
		od_server_sync_request(server, 1);
		break;
	default:
		break;
	}

	/* update server stats */
	od_stat_query_start(&server->stats_state);
	return OD_OK;
}

static void od_frontend_remote_server_on_read(od_relay_t *relay, int size)
{
	od_stat_t *stats = relay->on_read_arg;
	od_stat_recv_server(stats, size);
}

static void od_frontend_remote_client_on_read(od_relay_t *relay, int size)
{
	od_stat_t *stats = relay->on_read_arg;
	od_stat_recv_client(stats, size);
}

static od_frontend_status_t od_frontend_ctl(od_client_t *client)
{
	uint32_t op = od_client_ctl_of(client);
	if (op & OD_CLIENT_OP_KILL) {
		od_client_ctl_unset(client, OD_CLIENT_OP_KILL);
		od_client_notify_read(client);
		return OD_STOP;
	}
	return OD_OK;
}

static od_frontend_status_t od_frontend_remote(od_client_t *client)
{
	od_route_t *route = client->route;
	client->cond = machine_cond_create();

	if (client->cond == NULL) {
		return OD_EOOM;
	}

	/* enable client notification mechanism */
	int rc;
	rc = machine_read_start(client->notify_io, client->cond);
	if (rc == -1) {
		return OD_ECLIENT_READ;
	}

	od_frontend_status_t status =
		od_relay_start(&client->relay, client->cond, OD_ECLIENT_READ,
			       OD_ESERVER_WRITE,
			       od_frontend_remote_client_on_read, &route->stats,
			       od_frontend_remote_client, client);
	if (status != OD_OK) {
		return status;
	}

	od_server_t *server = NULL;
	for (;;) {
		for (;;) {
			if (od_should_drop_connection(client, server)) {
				/* Odyssey is in a state of completion, we done 
                         * the last client's request and now we can drop the connection  */

				/* a sort of EAGAIN */
				status = OD_ECLIENT_READ;
				break;
			}
			/* one minute */
			if (machine_cond_wait(client->cond, 60000) == 0) {
				break;
			}
		}

		if (od_frontend_status_is_err(status))
			break;

		/* client operations */
		status = od_frontend_ctl(client);
		if (status != OD_OK)
			break;

		server = client->server;
		od_dbg_printf_on_dvl_lvl(1, "pair now %s\n", client->id.id);
		if (server) {
			od_dbg_printf_on_dvl_lvl(1, "pair now %s\n",
						 server->id.id);
		}
		/* attach */
		status = od_relay_step(&client->relay);
		if (status == OD_ATTACH) {
			od_dbg_printf_on_dvl_lvl(1, "attaching %s\n",
						 client->id);
			assert(server == NULL);
			status = od_frontend_attach_and_deploy(client, "main");
			if (status != OD_OK)
				break;
			server = client->server;
			status = od_relay_start(
				&server->relay, client->cond, OD_ESERVER_READ,
				OD_ECLIENT_WRITE,
				od_frontend_remote_server_on_read,
				&route->stats, od_frontend_remote_server,
				client);
			if (status != OD_OK)
				break;
			od_relay_attach(&client->relay, &server->io);
			od_relay_attach(&server->relay, &client->io);

			/* retry read operation after attach */
			continue;
		} else if (status != OD_OK) {
			break;
		} else {
			od_dbg_printf_on_dvl_lvl(
				1, "continue %s\n",
				od_frontend_status_to_str(status));
		}

		if (server == NULL)
			continue;

		status = od_relay_step(&server->relay);
		if (status == OD_DETACH) {
			/* detach on transaction pooling  */
			od_dbg_printf_on_dvl_lvl(1, "detaching %s \n",
						 client->id.id);
			/* write any pending data to server first */
			od_frontend_status_t flush_status;
			flush_status = od_relay_flush(&server->relay);
			if (flush_status != OD_OK)
				break;
			od_relay_detach(&client->relay);
			od_relay_stop(&server->relay);

			/* cleanup server */
			rc = od_reset(server);
			if (rc == -1) {
				flush_status = OD_ESERVER_WRITE;
				break;
			}

			/* push server connection back to route pool */
			od_router_t *router = client->global->router;
			od_router_detach(router, client);
			server = NULL;
		} else if (status != OD_OK) {
			break;
		} else {
			od_dbg_printf_on_dvl_lvl(
				1, "continue %s\n",
				od_frontend_status_to_str(status));
		}
	}

	if (client->server) {
		od_server_t *curr_server = client->server;

		od_frontend_status_t flush_status;
		flush_status = od_relay_flush(&curr_server->relay);
		od_relay_stop(&curr_server->relay);
		if (flush_status != OD_OK) {
			return flush_status;
		}

		flush_status = od_relay_flush(&client->relay);
		if (flush_status != OD_OK) {
			return flush_status;
		}
	}

	od_relay_stop(&client->relay);
	return status;
}

static void od_frontend_cleanup(od_client_t *client, char *context,
				od_frontend_status_t status,
				od_error_logger_t *l)
{
	od_instance_t *instance = client->global->instance;
	od_router_t *router = client->global->router;
	od_route_t *route = client->route;
	char peer[128];
	int rc;

	od_server_t *server = client->server;

	if (od_frontend_status_is_err(status)) {
		od_error_logger_store_err(l, status);

		if (route->extra_logging_enabled &&
		    !od_route_is_dynamic(route)) {
			od_error_logger_store_err(route->err_logger, status);
		}
	}

	switch (status) {
	case OD_STOP:
	/* fallthrough */
	case OD_OK:
		/* graceful disconnect or kill */
		if (instance->config.log_session) {
			od_log(&instance->logger, context, client, server,
			       "client disconnected (route %s.%s)",
			       route->rule->db_name, route->rule->user_name);
		}
		if (!client->server)
			break;

		rc = od_reset(server);
		if (rc != 1) {
			/* close backend connection */
			od_router_close(router, client);
			break;
		}
		/* push server to router server pool */
		od_router_detach(router, client);
		break;

	case OD_EOOM:
		od_error(&instance->logger, context, client, server, "%s",
			 "memory allocation error");
		if (client->server)
			od_router_close(router, client);
		break;

	case OD_EATTACH:
		assert(server == NULL);
		assert(client->route != NULL);
		od_frontend_error(client, KIWI_CONNECTION_FAILURE,
				  "failed to get remote server connection");
		break;

	case OD_EATTACH_TOO_MANY_CONNECTIONS:
		assert(server == NULL);
		assert(client->route != NULL);
		od_frontend_error(
			client, KIWI_TOO_MANY_CONNECTIONS,
			"too many active clients for user (pool_size for "
			"user %s.%s reached %d)",
			client->startup.database.value,
			client->startup.user.value,
			client->rule != NULL ? client->rule->pool_size : -1);
		break;

	case OD_ECLIENT_READ:
		/*fallthrough*/
	case OD_ECLIENT_WRITE:
		/* close client connection and reuse server
			 * link in case of client errors */

		od_getpeername(client->io.io, peer, sizeof(peer), 1, 1);
		od_log(&instance->logger, context, client, server,
		       "client disconnected (read/write error, addr %s): %s, status %s",
		       peer, od_io_error(&client->io),
		       od_frontend_status_to_str(status));
		if (!client->server)
			break;
		rc = od_reset(server);
		if (rc != 1) {
			/* close backend connection */
			od_log(&instance->logger, context, client, server,
			       "reset unsuccessful, closing server connection");
			od_router_close(router, client);
			break;
		}
		/* push server to router server pool */
		od_router_detach(router, client);
		break;

	case OD_ESERVER_CONNECT:
		/* server attached to client and connection failed */
		if (server->error_connect && route->rule->client_fwd_error) {
			/* forward server error to client */
			od_frontend_error_fwd(client);
		} else {
			od_frontend_error(
				client, KIWI_CONNECTION_FAILURE,
				"failed to connect to remote server %s%.*s",
				server->id.id_prefix,
				(int)sizeof(server->id.id), server->id.id);
		}
		/* close backend connection */
		od_router_close(router, client);
		break;

	case OD_ESERVER_READ:
	case OD_ESERVER_WRITE:
		/* close client connection and close server
			 * connection in case of server errors */
		od_log(&instance->logger, context, client, server,
		       "server disconnected (read/write error): %s, status %s",
		       od_io_error(&server->io),
		       od_frontend_status_to_str(status));
		od_frontend_error(client, KIWI_CONNECTION_FAILURE,
				  "remote server read/write error %s%.*s",
				  server->id.id_prefix, sizeof(server->id.id),
				  server->id.id);
		/* close backend connection */
		od_router_close(router, client);
		break;
	case OD_UNDEF:
	case OD_SKIP:
	case OD_ATTACH:
	/* fallthrough */
	case OD_DETACH:
	case OD_ESYNC_BROKEN:
		od_error(&instance->logger, context, client, server,
			 "unexpected error status %s (%d)",
			 od_frontend_status_to_str(status), (uint32)status);
		od_router_close(router, client);
		break;
	default:
		od_error(
			&instance->logger, context, client, server,
			"unexpected error status %s (%d), possible corruption, abort()",
			od_frontend_status_to_str(status), (uint32)status);
		abort();
	}
}

static void od_application_name_add_host(od_client_t *client)
{
	if (client == NULL || client->io.io == NULL)
		return;
	char app_name_with_host[KIWI_MAX_VAR_SIZE];
	char peer_name[KIWI_MAX_VAR_SIZE];
	int app_name_len = 7;
	char *app_name = "unknown";
	kiwi_var_t *app_name_var =
		kiwi_vars_get(&client->vars, KIWI_VAR_APPLICATION_NAME);
	if (app_name_var != NULL) {
		app_name_len = app_name_var->value_len;
		app_name = app_name_var->value;
	}
	od_getpeername(client->io.io, peer_name, sizeof(peer_name), 1,
		       0); // return code ignored

	int length =
		od_snprintf(app_name_with_host, KIWI_MAX_VAR_SIZE, "%.*s - %s",
			    app_name_len, app_name, peer_name);
	kiwi_vars_set(&client->vars, KIWI_VAR_APPLICATION_NAME,
		      app_name_with_host,
		      length + 1); // return code ignored
}

void od_frontend(void *arg)
{
	od_client_t *client = arg;
	od_instance_t *instance = client->global->instance;
	od_router_t *router = client->global->router;
	od_module_t *modules = client->global->modules;

	/* log client connection */
	if (instance->config.log_session) {
		char peer[128];
		od_getpeername(client->io.io, peer, sizeof(peer), 1, 1);
		od_log(&instance->logger, "startup", client, NULL,
		       "new client connection %s", peer);
	}

	/* attach client io to worker machine event loop */
	int rc;
	rc = od_io_attach(&client->io);
	if (rc == -1) {
		od_error(&instance->logger, "startup", client, NULL,
			 "failed to transfer client io");
		od_io_close(&client->io);
		machine_close(client->notify_io);
		od_client_free(client);
		od_atomic_u32_dec(&router->clients_routing);
		return;
	}

	rc = machine_io_attach(client->notify_io);
	if (rc == -1) {
		od_error(&instance->logger, "startup", client, NULL,
			 "failed to transfer client notify io");
		od_io_close(&client->io);
		machine_close(client->notify_io);
		od_client_free(client);
		od_atomic_u32_dec(&router->clients_routing);
		return;
	}

	/* ensure global client_max limit */
	uint32_t clients = od_atomic_u32_inc(&router->clients);
	if (instance->config.client_max_set &&
	    clients >= (uint32_t)instance->config.client_max) {
		od_frontend_error(
			client, KIWI_TOO_MANY_CONNECTIONS,
			"too many tcp connections (global client_max %d)",
			instance->config.client_max);
		od_frontend_close(client);
		od_atomic_u32_dec(&router->clients_routing);
		return;
	}

	/* handle startup */
	rc = od_frontend_startup(client);
	if (rc == -1) {
		od_frontend_close(client);
		od_atomic_u32_dec(&router->clients_routing);
		return;
	}

	/* handle cancel request */
	if (client->startup.is_cancel) {
		od_log(&instance->logger, "startup", client, NULL,
		       "cancel request");
		od_router_cancel_t cancel;
		od_router_cancel_init(&cancel);
		rc = od_router_cancel(router, &client->startup.key, &cancel);
		if (rc == 0) {
			od_cancel(client->global, cancel.storage, &cancel.key,
				  &cancel.id);
			od_router_cancel_free(&cancel);
		}
		od_frontend_close(client);
		od_atomic_u32_dec(&router->clients_routing);
		return;
	}

	/* Use client id as backend key for the client.
	 *
	 * This key will be used to identify a server by
	 * user cancel requests. The key must be regenerated
	 * for each new client-server assignment, to avoid
	 * possibility of cancelling requests by a previous
	 * server owners.
	 */
	client->key.key_pid = client->id.id_a;
	client->key.key = client->id.id_b;

	/* route client */
	od_router_status_t router_status;
	router_status = od_router_route(router, client);

	/* routing is over */
	od_atomic_u32_dec(&router->clients_routing);

	if (router_status == OD_ROUTER_OK) {
		od_route_t *route = client->route;
		if (route->rule->application_name_add_host)
			od_application_name_add_host(client);
		if (instance->config.log_session) {
			od_log(&instance->logger, "startup", client, NULL,
			       "route '%s.%s' to '%s.%s'",
			       client->startup.database.value,
			       client->startup.user.value, route->rule->db_name,
			       route->rule->user_name);
		}
	} else {
		char peer[128];
		od_getpeername(client->io.io, peer, sizeof(peer), 1, 1);

		if (od_router_status_is_err(router_status)) {
			od_error_logger_store_err(router->router_err_logger,
						  router_status);
		}

		switch (router_status) {
		case OD_ROUTER_ERROR:
			od_error(&instance->logger, "startup", client, NULL,
				 "routing failed for '%s' client, closing",
				 peer);
			od_frontend_error(client, KIWI_SYSTEM_ERROR,
					  "client routing failed");
			break;
		case OD_ROUTER_ERROR_NOT_FOUND:
			od_error(
				&instance->logger, "startup", client, NULL,
				"route for '%s.%s' is not found for '%s' client, closing",
				client->startup.database.value,
				client->startup.user.value, peer);
			od_frontend_error(client, KIWI_UNDEFINED_DATABASE,
					  "route for '%s.%s' is not found",
					  client->startup.database.value,
					  client->startup.user.value);
			break;
		case OD_ROUTER_ERROR_LIMIT:
			od_error(
				&instance->logger, "startup", client, NULL,
				"global connection limit reached for '%s' client, closing",
				peer);

			od_frontend_error(
				client, KIWI_TOO_MANY_CONNECTIONS,
				"too many client tcp connections (global client_max)");
			break;
		case OD_ROUTER_ERROR_LIMIT_ROUTE:
			od_error(
				&instance->logger, "startup", client, NULL,
				"route connection limit reached for client '%s', closing",
				peer);
			od_frontend_error(
				client, KIWI_TOO_MANY_CONNECTIONS,
				"too many client tcp connections (client_max for user %s.%s "
				"%d)",
				client->startup.database.value,
				client->startup.user.value,
				client->rule != NULL ?
					client->rule->client_max :
					-1);
			break;
		case OD_ROUTER_ERROR_REPLICATION:
			od_error(
				&instance->logger, "startup", client, NULL,
				"invalid value for parameter \"replication\" for client '%s'",
				peer);

			od_frontend_error(
				client, KIWI_CONNECTION_FAILURE,
				"invalid value for parameter \"replication\"");
			break;
		default:
			assert(0);
			break;
		}

		od_frontend_close(client);
		return;
	}

	/* pre-auth callback */
	od_list_t *i;
	od_list_foreach(&modules->link, i)
	{
		od_module_t *module;
		module = od_container_of(i, od_module_t, link);
		if (module->auth_attempt_cb(client) ==
		    OD_MODULE_CB_FAIL_RETCODE) {
			goto cleanup;
		}
	}

	/* client authentication */
	rc = od_auth_frontend(client);

	if (rc != OK_RESPONSE) {
		goto cleanup;
	}

	/* auth result callback */
	od_list_foreach(&modules->link, i)
	{
		od_module_t *module;
		module = od_container_of(i, od_module_t, link);

		if (rc == OK_RESPONSE) {
			rc = module->auth_complete_cb(client, rc);
			if (rc != OD_MODULE_CB_OK_RETCODE) {
				// user blocked from module callback
				goto cleanup;
			}
		} else {
			/* rc == -1
			 * here we ignore module retcode because auth already failed
			 * we just inform side modules that usr was trying to log in
			 */
			module->auth_complete_cb(client, rc);
			goto cleanup;
		}
	}

	/* setup client and run main loop */
	od_route_t *route = client->route;

	od_frontend_status_t status;
	status = OD_UNDEF;
	switch (route->rule->storage->storage_type) {
	case OD_RULE_STORAGE_LOCAL: {
		status = od_frontend_local_setup(client);
		if (status != OD_OK)
			break;

		status = od_frontend_local(client);
		break;
	}
	case OD_RULE_STORAGE_REMOTE: {
		status = od_frontend_setup(client);
		if (status != OD_OK)
			break;

		status = od_frontend_remote(client);

		break;
	}
	}
	od_error_logger_t *l;
	l = router->route_pool.err_logger;

	od_frontend_cleanup(client, "main", status, l);

	od_list_foreach(&modules->link, i)
	{
		od_module_t *module;
		module = od_container_of(i, od_module_t, link);
		module->disconnect_cb(client, status);
	}

	/* cleanup */

cleanup:
	/* detach client from its route */
	od_router_unroute(router, client);
	/* close frontend connection */
	od_frontend_close(client);
}
