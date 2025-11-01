
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
	od_client_free(client);
}

int od_frontend_info(od_client_t *client, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	machine_msg_t *msg;
	msg = od_frontend_info_msg(client, NULL, fmt, args);
	va_end(args);
	if (msg == NULL) {
		return -1;
	}
	return od_write(&client->io, msg);
}

int od_frontend_error(od_client_t *client, char *code, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	machine_msg_t *msg;
	msg = od_frontend_error_msg(client, NULL, code, fmt, args);
	va_end(args);
	if (msg == NULL) {
		return -1;
	}
	return od_write(&client->io, msg);
}

int od_frontend_fatal(od_client_t *client, char *code, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	machine_msg_t *msg;
	msg = od_frontend_fatal_msg(client, NULL, code, fmt, args);
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

	for (uint32_t startup_attempt = 0;
	     startup_attempt < MAX_STARTUP_ATTEMPTS; startup_attempt++) {
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
		goto error;

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

static inline int candidate_cmp_desc(const void *v1, const void *v2)
{
	const od_endpoint_attach_candidate_t *c1 = v1;
	const od_endpoint_attach_candidate_t *c2 = v2;

	return c2->priority - c1->priority;
}

static inline int od_frontend_attach_candidate_get_priority(
	od_instance_t *instance, od_rule_storage_t *storage,
	od_endpoint_attach_candidate_t *candidate,
	od_target_session_attrs_t tsa, int prefer_localhost)
{
	/*
	 * priority of endpoints is determined by (from highest to lowest):
	 * - prefer_localhost: for serice accounts connections
	 * - az: we should use same az as of odyssey instance if it is possible
	 * - tsa match: even if it is outdated, rw state of endpoint doesn't change frequently
	 * - random number: to select host randomly between several suitable hosts
	 * 
	 * so:
	 * + 1000 priority by localhost address
	 * + 500 priority by matched az
	 * + 200 priority by matched tsa
	 * + [0..100) shuffle coeff
	 * 
	 * also negative priority will mean endpoints, that is not suitable,
	 * ex: endpoints which read-write status certanly doesn't fit tsa
	 */

	od_storage_endpoint_t *endpoint = candidate->endpoint;

	int priority = 0;

	priority += machine_lrand48() % 100;

	od_storage_endpoint_status_t status;
	od_storage_endpoint_status_init(&status);
	od_storage_endpoint_status_get(&endpoint->status, &status);

	int status_is_recent = !od_storage_endpoint_status_is_outdated(
		&status, storage->endpoints_status_poll_interval_ms);
	int tsa_match = od_tsa_match_rw_state(tsa, status.is_read_write);

	if (status_is_recent && !tsa_match) {
		return -1;
	}

	if (tsa_match) {
		priority += 200;
	}

	if (strcmp(instance->config.availability_zone,
		   candidate->endpoint->address.availability_zone) == 0) {
		priority += 500;
	}

	if (prefer_localhost && od_address_is_localhost(&endpoint->address)) {
		priority += 1000;
	}

	return priority;
}

void od_frontend_attach_init_candidates(
	od_instance_t *instance, od_rule_storage_t *storage,
	od_endpoint_attach_candidate_t *candidates,
	od_target_session_attrs_t tsa, int prefer_localhost)
{
	size_t count = storage->endpoints_count;

	for (size_t i = 0; i < count; ++i) {
		candidates[i].endpoint = &storage->endpoints[i];
		candidates[i].priority = 0;
	}

	if (count == 1) {
		return;
	}

	for (size_t i = 0; i < count; ++i) {
		candidates[i].priority =
			od_frontend_attach_candidate_get_priority(
				instance, storage, &candidates[i], tsa,
				prefer_localhost);
	}

	qsort(candidates, count, sizeof(od_endpoint_attach_candidate_t),
	      candidate_cmp_desc);
}

static inline od_frontend_status_t od_frontend_attach_to_endpoint(
	od_client_t *client, char *context, kiwi_params_t *route_params,
	od_storage_endpoint_t *endpoint, od_target_session_attrs_t tsa)
{
	od_instance_t *instance = client->global->instance;
	od_router_t *router = client->global->router;
	od_route_t *route = client->route;

	bool wait_for_idle = false;
	for (;;) {
		od_router_status_t status;
		status = od_router_attach(router, client, wait_for_idle,
					  &endpoint->address);
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
			 "client %s%.*s attached to %s%.*s",
			 client->id.id_prefix, (int)sizeof(client->id.id),
			 client->id.id, server->id.id_prefix,
			 (int)sizeof(server->id.id), server->id.id);

		assert(od_server_synchronized(server));
		assert(server->relay.iov == 0 ||
		       !machine_iov_pending(server->relay.iov));

		/* connect to server, if necessary */
		if (od_backend_not_connected(server)) {
			int rc;
			od_atomic_u32_inc(&router->servers_routing);

			assert(client->config_listen != NULL);
			rc = od_backend_connect(server, context, route_params,
						client);

			od_atomic_u32_dec(&router->servers_routing);
			if (rc == NOT_OK_RESPONSE) {
				/* In case of 'too many connections' error, retry attach attempt by
				* waiting for a idle server connection for pool_timeout ms
				*/
				wait_for_idle =
					route->rule->pool->timeout > 0 &&
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
		}

		int rc = od_backend_startup_preallocated(server, route_params,
							 client);
		if (rc != OK_RESPONSE) {
			return OD_ESERVER_CONNECT;
		}

		if (od_backend_check_tsa(endpoint, context, server, client,
					 tsa) != OK_RESPONSE) {
			char addr[256];
			od_address_to_str(&endpoint->address, addr,
					  sizeof(addr) - 1);
			od_log(&instance->logger, context, client, server,
			       "read-write status of '%s' is mismatched with expected '%s' or failed to update",
			       addr, od_target_session_attrs_to_str(tsa));

			/* push server to router server pool */
			od_router_detach(router, client);

			/* try another host */
			return OD_EATTACH_TARGET_SESSION_ATTRS_MISMATCH;
		}

		return OD_OK;
	}
}

static inline od_frontend_status_t
od_frontend_attach(od_client_t *client, char *context,
		   kiwi_params_t *route_params)
{
	od_instance_t *instance = client->global->instance;
	od_route_t *route = client->route;
	od_rule_storage_t *storage = route->rule->storage;

	if (route->rule->pool->reserve_prepared_statement) {
		client->relay.require_full_prep_stmt = 1;
	}

	od_target_session_attrs_t tsa = od_tsa_get_effective(client);

	od_endpoint_attach_candidate_t candidates[OD_STORAGE_MAX_ENDPOINTS];
	od_frontend_attach_init_candidates(instance, storage, candidates, tsa,
					   0 /* prefer localhost */);

	od_frontend_status_t status = OD_EATTACH;

	for (size_t i = 0; i < storage->endpoints_count; ++i) {
		od_storage_endpoint_t *endpoint = candidates[i].endpoint;

		if (candidates[i].priority >= 0) {
			status = od_frontend_attach_to_endpoint(
				client, context, route_params, endpoint, tsa);
		} else {
			/* connection attempt will fail anyway */
			status = OD_EATTACH_TARGET_SESSION_ATTRS_MISMATCH;
		}

		if (status == OD_OK) {
			return status;
		}

		char addr[256];
		od_address_to_str(&endpoint->address, addr, sizeof(addr) - 1);

		od_debug(&instance->logger, context, client, NULL,
			 "attach to %s failed with status: %s", addr,
			 od_frontend_status_to_str(status));
	}

	return status;
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
	if (client->rule->maintain_params) {
		int rc;
		rc = od_deploy(client, context);
		if (rc == -1)
			return OD_ESERVER_WRITE;
		/* set number of replies to discard */
		server->deploy_sync = rc;

		od_server_sync_request(server, server->deploy_sync);
	}

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

		// close backend connection
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
	od_route_t *route = client->route;

	/* set parameters */
	od_frontend_status_t status;
	status = od_frontend_setup_params(client);
	if (status != OD_OK)
		return status;

	if (route->rule->pool->reserve_prepared_statement) {
		if (od_client_init_hm(client) != OK_RESPONSE) {
			od_log(&instance->logger, "setup", client, NULL,
			       "failed to initialize hash map for prepared statements");
			return OD_EOOM;
		}
	}

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
		od_log(&instance->logger, "setup", client, NULL,
		       "client connection from %s to route %s.%s accepted",
		       client->peer, route->rule->db_name,
		       route->rule->user_name);
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
	char data[128];
	int data_len;
	/* current version and build */
	data_len =
		od_snprintf(data, sizeof(data), "%s-%s-%s", OD_VERSION_NUMBER,
			    OD_VERSION_GIT, OD_VERSION_BUILD);
	msg = kiwi_be_write_parameter_status(stream, "server_version", 15, data,
					     data_len + 1);
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
	od_config_t *config = &instance->config;

	if (!config->online_restart_drop_options.drop_enabled) {
		return false;
	}

	if (server == NULL) {
		/* server is null - client was never attached to any server so its ok to eject this conn  */
		return true;
	}
	od_thread_global **gl = od_thread_global_get();
	if (gl == NULL) {
		od_log(&instance->logger, "shutdown", client, server,
		       "drop client connection on restart, unable to throttle (wid %d)",
		       (*gl)->wid);
		/* this is clearly something bad, TODO: handle properly */
		return true;
	}

	od_conn_eject_info *info = (*gl)->info;

	uint32_t now_sec = machine_timeofday_sec();
	bool res = false;

	pthread_mutex_lock(&info->mu);
	{
		if (info->last_conn_drop_ts + 1 < now_sec) {
			res = true;

			od_log(&instance->logger, "shutdown", client, server,
			       "drop client connection on restart (wid %d, last eject %d, curr time %d)",
			       (*gl)->wid, info->last_conn_drop_ts, now_sec);

			info->last_conn_drop_ts = now_sec;
		} else {
			od_debug(
				&instance->logger, "shutdown", client, server,
				"delay drop client connection on restart, last drop was too recent (wid %d, last drop %d, curr time %d)",
				(*gl)->wid, info->last_conn_drop_ts, now_sec);
		}
	}
	pthread_mutex_unlock(&info->mu);

	return res;
}

static inline bool od_eject_conn_with_timeout(od_client_t *client,
					      __attribute__((unused))
					      od_server_t *server,
					      uint64_t timeout)
{
	assert(server != NULL);
	od_dbg_printf_on_dvl_lvl(1, "current time %lld, drop horizon %lld\n",
				 machine_time_us(),
				 client->time_last_active + timeout);

	if (client->time_last_active + timeout < machine_time_us()) {
		return true;
	}

	return false;
}

static od_frontend_status_t od_frontend_ctl(od_client_t *client)
{
	if (od_atomic_u64_of(&client->killed) == 1) {
		return OD_STOP;
	}

	return OD_OK;
}

static inline od_frontend_status_t
od_process_drop_on_restart(od_client_t *client)
{
	//TODO:: drop no more than X connection per sec/min/whatever
	od_instance_t *instance = client->global->instance;
	od_server_t *server = client->server;

	if (od_likely(instance->shutdown_worker_id == INVALID_COROUTINE_ID)) {
		// try to optimize likely path
		return OD_OK;
	}

	if (od_unlikely(client->rule->storage->storage_type ==
			OD_RULE_STORAGE_LOCAL)) {
		/* local server is not very important (db like console, pgbouncer used for stats)*/
		return OD_ECLIENT_READ;
	}

	if (od_unlikely(server == NULL)) {
		if (od_eject_conn_with_rate(client, server, instance)) {
			return OD_ECLIENT_READ;
		}
		return OD_OK;
	}

	if (server->state ==
		    OD_SERVER_ACTIVE /* we can drop client that are just connected and do not perform any queries */
	    && !od_server_synchronized(server)) {
		/* most probably we are not in transaction, but still executing some stmt */
		return OD_OK;
	}

	if (od_unlikely(!server->is_transaction)) {
		if (od_eject_conn_with_rate(client, server, instance)) {
			return OD_ECLIENT_READ;
		}
		return OD_OK;
	}

	return OD_OK;
}

static inline od_frontend_status_t
od_process_drop_transaction_pool(od_client_t *client)
{
	return od_process_drop_on_restart(client);
}

static inline od_frontend_status_t
od_process_drop_on_client_idle_timeout(od_client_t *client, od_server_t *server)
{
	od_instance_t *instance = client->global->instance;

	// as we do not unroute client in session pooling after transaction block etc
	// we should consider this case separately
	// general logic is: if client do nothing long enough we can assume this is just a stale connection
	// but we need to ensure this connection was initialized etc
	if (od_unlikely(
		    server != NULL && !server->is_transaction &&
		    /* case when we are out of any transactional block ut perform some stmt */
		    od_server_synchronized(server))) {
		if (od_eject_conn_with_timeout(
			    client, server,
			    client->rule->pool->client_idle_timeout)) {
			od_log(&instance->logger, "shutdown", client, server,
			       "drop idle client connection on due timeout %d sec",
			       client->rule->pool->client_idle_timeout);

			return OD_ECLIENT_READ;
		}
	}

	return OD_OK;
}

static inline od_frontend_status_t
od_process_drop_on_idle_in_transaction(od_client_t *client, od_server_t *server)
{
	od_instance_t *instance = client->global->instance;

	// the same as above but we are going to drop client inside transaction block
	if (server != NULL && server->is_transaction &&
	    /*server is sync - that means client executed some stmts and got get result, and now just... do nothing */
	    od_server_synchronized(server)) {
		if (od_eject_conn_with_timeout(
			    client, server,
			    client->rule->pool->idle_in_transaction_timeout)) {
			od_log(&instance->logger, "shutdown", client, server,
			       "drop idle in transaction connection on due timeout %d sec",
			       client->rule->pool->idle_in_transaction_timeout);

			return OD_ECLIENT_READ;
		}
	}

	return OD_OK;
}

static inline bool od_process_should_drop_session_on_pause(od_client_t *client,
							   od_server_t *server)
{
	if (!od_global_is_paused(client->global)) {
		return false;
	}

	od_route_t *route = client->route;

	/* do not drop console clients */
	if (route->rule->storage->storage_type == OD_RULE_STORAGE_LOCAL) {
		return false;
	}

	if (server == NULL) {
		return true;
	}

	if (!od_server_synchronized(server)) {
		return false;
	}

	if (server->offline || !server->is_transaction) {
		return true;
	}

	return false;
}

static inline od_frontend_status_t
od_process_drop_session_pool(od_client_t *client)
{
	od_frontend_status_t status;
	od_server_t *server = client->server;

	if (od_process_should_drop_session_on_pause(client, server)) {
		od_instance_t *instance = client->global->instance;
		od_log(&instance->logger, "pause", client, server,
		       "drop client connection on pause");

		return OD_ECLIENT_READ;
	}

	if (od_unlikely(client->rule->pool->client_idle_timeout)) {
		status = od_process_drop_on_client_idle_timeout(client, server);
		if (status != OD_OK) {
			return status;
		}
	}

	if (od_unlikely(client->rule->pool->idle_in_transaction_timeout)) {
		status = od_process_drop_on_idle_in_transaction(client, server);
		if (status != OD_OK) {
			return status;
		}
	}

	status = od_process_drop_on_restart(client);

	return status;
}

static inline od_frontend_status_t
od_process_connection_drop(od_client_t *client)
{
	od_frontend_status_t status = od_frontend_ctl(client);
	if (status != OD_OK) {
		return status;
	}

	switch (client->rule->pool->pool_type) {
	case OD_RULE_POOL_SESSION:
		return od_process_drop_session_pool(client);
	case OD_RULE_POOL_STATEMENT:
	case OD_RULE_POOL_TRANSACTION:
		return od_process_drop_transaction_pool(client);
	default:
		abort();
	}

	return OD_OK;
}

static od_frontend_status_t od_frontend_local(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;

	od_frontend_status_t status;

	for (;;) {
		machine_msg_t *msg = NULL;
		for (;;) {
			/* local server is always null */
			assert(client->server == NULL);
			status = od_process_connection_drop(client);
			if (status != OD_OK) {
				/* Odyssey is in a state of completion, we done
				 * the last client's request and now we can drop the connection
				 */
				return status;
			}
			/* one minute */
			msg = od_read(&client->io, 60000);

			if (machine_timedout()) {
				/* retry wait to recheck exit condition */
				assert(msg == NULL);
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
		msg = kiwi_be_write_ready(stream, KIWI_BE_EMPTY_QUERY_RESPONSE);
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

static inline bool od_frontend_should_detach_transaction(od_server_t *server)
{
	return !server->is_transaction;
}

static inline bool od_frontend_should_detach_session(od_server_t *server)
{
	return (server->offline || od_global_is_paused(server->global)) &&
	       !server->is_transaction;
}

static inline bool
od_frontend_should_detach_on_ready_for_query(od_route_t *route,
					     od_server_t *server)
{
	switch (route->rule->pool->pool_type) {
	case OD_RULE_POOL_STATEMENT:
		return true;
	case OD_RULE_POOL_TRANSACTION:
		return od_frontend_should_detach_transaction(server);
	case OD_RULE_POOL_SESSION:
		return od_frontend_should_detach_session(server);
	default:
		abort();
	}
}

static od_frontend_status_t od_frontend_remote_server(od_relay_t *relay,
						      char *data, int size)
{
	od_client_t *client = relay->client;
	od_server_t *server = client->server;
	od_route_t *route = client->route;
	od_instance_t *instance = client->global->instance;
	od_frontend_status_t retstatus;

	retstatus = OD_OK;

	kiwi_be_type_t type = *data;
	if (instance->config.log_debug)
		od_debug(&instance->logger, "main", client, server, "%s",
			 kiwi_be_type_to_string(type));

	int is_deploy = od_server_in_deploy(server);
	int is_ready_for_query = 0;

	int rc;
	switch (type) {
	case KIWI_BE_ERROR_RESPONSE:

		if (od_server_in_sync_point(server)) {
			server->sync_point_deploy_msg = NULL;
		}
		od_backend_error(server, "main", data, size);
		break;
	case KIWI_BE_PARAMETER_STATUS:
		rc = od_backend_update_parameter(server, "main", data, size, 0);
		if (rc == -1)
			return relay->error_read;
		break;
	case KIWI_BE_COPY_IN_RESPONSE:
	case KIWI_BE_COPY_OUT_RESPONSE:
		server->in_out_response_received++;
		break;
	case KIWI_BE_COPY_DONE:
		/* should go after copy out
		* states that backend copy ended
		*/
		server->done_fail_response_received++;
		break;
	case KIWI_BE_COPY_FAIL:
		/*
		* states that backend copy failed
		*/
		return relay->error_write;
	case KIWI_BE_READY_FOR_QUERY: {
		is_ready_for_query = 1;
		od_backend_ready(server, data, size);

		/* exactly one RFQ! */
		if (od_server_in_sync_point(server)) {
			retstatus = OD_SKIP;
		}

		if (is_deploy)
			server->deploy_sync--;

		if (!server->synced_settings) {
			server->synced_settings = true;
			break;
		}
		/* update server stats */
		int64_t query_time = 0;
		od_stat_query_end(&route->stats, &server->stats_state,
				  server->is_transaction, &query_time);
		if (instance->config.log_debug && query_time > 0) {
			od_debug(&instance->logger, "main", server->client,
				 server, "query time: %" PRIi64 " microseconds",
				 query_time);
		}

		break;
	}
	case KIWI_BE_PARSE_COMPLETE:
		if (route->rule->pool->reserve_prepared_statement) {
			// skip msg
			retstatus = OD_SKIP;
		}
	default:
		break;
	}

	/* discard replies during configuration deploy */
	if (is_deploy || retstatus == OD_SKIP)
		return OD_SKIP;

	if (route->id.physical_rep || route->id.logical_rep) {
		// do not detach server connection on replication
		// the exceptional case in offine: we are going to shut down here
		if (server->offline) {
			return OD_DETACH;
		}
	} else {
		if (is_ready_for_query && od_server_synchronized(server) &&
		    server->parse_msg == NULL) {
			if (od_frontend_should_detach_on_ready_for_query(
				    route, server)) {
				return OD_DETACH;
			}
		}
	}

	return retstatus;
}

static inline od_retcode_t od_frontend_log_describe(od_instance_t *instance,
						    od_client_t *client,
						    char *data, int size)
{
	uint32_t name_len;
	char *name;
	int rc;
	kiwi_fe_describe_type_t t;
	rc = kiwi_be_read_describe(data, size, &name, &name_len, &t);
	if (rc == -1)
		return NOT_OK_RESPONSE;

	od_log(&instance->logger, "describe", client, client->server,
	       "(%s) name: %.*s",
	       t == KIWI_FE_DESCRIBE_PORTAL ? "portal" : "statement", name_len,
	       name);
	return OK_RESPONSE;
}

static inline od_retcode_t od_frontend_log_execute(od_instance_t *instance,
						   od_client_t *client,
						   char *data, int size)
{
	uint32_t name_len;
	char *name;
	int rc;
	rc = kiwi_be_read_execute(data, size, &name, &name_len);
	if (rc == -1)
		return NOT_OK_RESPONSE;

	od_log(&instance->logger, "execute", client, client->server,
	       "name: %.*s", name_len, name);
	return OK_RESPONSE;
}

static inline od_retcode_t od_frontend_parse_close(char *data, int size,
						   char **name,
						   uint32_t *name_len,
						   kiwi_fe_close_type_t *type)
{
	int rc;
	rc = kiwi_be_read_close(data, size, name, name_len, type);
	if (rc == -1)
		return NOT_OK_RESPONSE;
	return OK_RESPONSE;
}

static inline od_retcode_t od_frontend_log_close(od_instance_t *instance,
						 od_client_t *client,
						 char *name, uint32_t name_len,
						 kiwi_fe_close_type_t type)
{
	switch (type) {
	case KIWI_FE_CLOSE_PORTAL:
		od_log(&instance->logger, "close", client, client->server,
		       "portal, name: %.*s", name_len, name);
		return OK_RESPONSE;
	case KIWI_FE_CLOSE_PREPARED_STATEMENT:
		od_log(&instance->logger, "close", client, client->server,
		       "prepared statement, name: %.*s", name_len, name);
		return OK_RESPONSE;
	default:
		od_log(&instance->logger, "close", client, client->server,
		       "unknown close type, name: %.*s", name_len, name);
		return NOT_OK_RESPONSE;
	}
}

static inline od_retcode_t od_frontend_log_parse(od_instance_t *instance,
						 od_client_t *client,
						 char *context, char *data,
						 int size)
{
	uint32_t query_len;
	char *query;
	uint32_t name_len;
	char *name;
	int rc;
	rc = kiwi_be_read_parse(data, size, &name, &name_len, &query,
				&query_len);
	if (rc == -1)
		return NOT_OK_RESPONSE;

	od_log(&instance->logger, context, client, client->server, "%.*s %.*s",
	       name_len, name, query_len, query);
	return OK_RESPONSE;
}

static inline od_retcode_t od_frontend_log_bind(od_instance_t *instance,
						od_client_t *client, char *ctx,
						char *data, int size)
{
	uint32_t name_len;
	char *name;
	int rc;
	rc = kiwi_be_read_bind_stmt_name(data, size, &name, &name_len);
	if (rc == -1)
		return NOT_OK_RESPONSE;

	od_log(&instance->logger, ctx, client, client->server, "bind %.*s",
	       name_len, name);
	return OK_RESPONSE;
}

// 8 hex
#define OD_HASH_LEN 9

static inline machine_msg_t *
od_frontend_rewrite_msg(char *data, int size, int opname_start_offset,
			int operator_name_len, char *opname, int opnamelen)
{
	machine_msg_t *msg =
		machine_msg_create(size - operator_name_len + opnamelen);
	char *rewrite_data = machine_msg_data(msg);

	// packet header
	memcpy(rewrite_data, data, opname_start_offset);
	// prefix for opname
	od_snprintf(rewrite_data + opname_start_offset, opnamelen, opname);
	// rest of msg
	memcpy(rewrite_data + opname_start_offset + opnamelen,
	       data + opname_start_offset + operator_name_len,
	       size - opname_start_offset - operator_name_len);
	// set proper size to package
	kiwi_header_set_size((kiwi_header_t *)rewrite_data,
			     size - operator_name_len + opnamelen);

	return msg;
}

static od_frontend_status_t od_frontend_deploy_prepared_stmt(
	od_server_t *server, __attribute__((unused)) od_relay_t *relay,
	char *ctx, char *data, int size /* to adcance or to write? */,
	od_hash_t body_hash, char *opname, int opnamelen)
{
	od_route_t *route = server->route;
	od_instance_t *instance = server->global->instance;
	od_client_t *client = server->client;

	od_hashmap_elt_t desc;
	desc.data = data;
	desc.len = size;

	od_debug(&instance->logger, ctx, client, server,
		 "statement: %.*s, hash: %08x", desc.len, desc.data, body_hash);

	int refcnt = 0;
	od_hashmap_elt_t value;
	value.data = &refcnt;
	value.len = sizeof(int);
	od_hashmap_elt_t *value_ptr = &value;

	// send parse msg if needed
	if (od_hashmap_insert(server->prep_stmts, body_hash, &desc,
			      &value_ptr) == 0) {
		od_debug(&instance->logger, ctx, client, server,
			 "deploy %.*s operator %.*s to server", desc.len,
			 desc.data, opnamelen, opname);
		// rewrite msg
		// allocate prepered statement under name equal to body hash

		od_stat_parse(&route->stats);

		machine_msg_t *pmsg;
		pmsg = kiwi_fe_write_parse_description(NULL, opname, opnamelen,
						       desc.data, desc.len);
		if (pmsg == NULL) {
			return OD_ESERVER_WRITE;
		}

		if (instance->config.log_query || route->rule->log_query) {
			od_frontend_log_parse(instance, client, "rewrite parse",
					      machine_msg_data(pmsg),
					      machine_msg_size(pmsg));
		}

		od_stat_parse(&route->stats);
		// msg deallocated here
		od_dbg_printf_on_dvl_lvl(1, "relay %p write msg %c\n", relay,
					 *(char *)machine_msg_data(pmsg));

		od_write(&server->io, pmsg);
		// advance?
		// machine_iov_add(relay->iov, pmsg);

		return OD_OK;
	} else {
		int *refcnt;
		refcnt = value_ptr->data;
		*refcnt = 1 + *refcnt;

		od_stat_parse_reuse(&route->stats);
		return OD_OK;
	}
}

static inline od_frontend_status_t
od_frontend_deploy_prepared_stmt_msg(od_server_t *server, od_relay_t *relay,
				     char *ctx)
{
	od_frontend_status_t rc;
	char *data = machine_msg_data(server->parse_msg);
	int size = machine_msg_size(server->parse_msg);

	od_hash_t body_hash = od_murmur_hash(data, size);
	char opname[OD_HASH_LEN];
	od_snprintf(opname, OD_HASH_LEN, "%08x", body_hash);
	rc = od_frontend_deploy_prepared_stmt(server, relay, ctx, data, size,
					      body_hash, opname, OD_HASH_LEN);

	machine_msg_free(server->parse_msg);
	server->parse_msg = NULL;
	return rc;
}

static od_frontend_status_t od_process_virtual_set(od_client_t *client,
						   od_parser_t *parser)
{
	od_token_t token;
	od_keyword_t *keyword;
	char *option_value;
	int option_value_len;
	int rc;
	od_server_t *server;
	od_instance_t *instance = client->global->instance;

	server = client->server;

	assert(server != NULL);

	/* need to read exact odyssey parameter here */
	rc = od_parser_next(parser, &token);
	if (rc != OD_PARSER_SYMBOL || token.value.num != '.') {
		return OD_OK;
	}

	rc = od_parser_next(parser, &token);
	switch (rc) {
	case OD_PARSER_KEYWORD:
		keyword = od_keyword_match(od_query_process_keywords, &token);
		if (keyword == NULL ||
		    keyword->id != OD_QUERY_PROCESSING_LTSA) {
			/* some other option, skip */
			return OD_OK;
		}
		break;
	default:
		return OD_OK;
	}

	rc = od_parser_next(parser, &token);
	if (rc != OD_PARSER_SYMBOL || token.value.num != '=') {
		return OD_OK;
	}

	rc = od_parser_next(parser, &token);

	if (rc != OD_PARSER_STRING) {
		return OD_OK;
	}

	option_value = token.value.string.pointer;
	option_value_len = token.value.string.size;

	/* for now, very straightforward logic, as there is only one supported param */
	if (strncasecmp(option_value, "read-only", option_value_len) == 0) {
		kiwi_vars_set(&client->vars,
			      KIWI_VAR_ODYSSEY_TARGET_SESSION_ATTRS,
			      option_value, option_value_len);
	} else if (strncasecmp(option_value, "read-write", option_value_len) ==
		   0) {
		kiwi_vars_set(&client->vars,
			      KIWI_VAR_ODYSSEY_TARGET_SESSION_ATTRS,
			      option_value, option_value_len);
	} else if (strncasecmp(option_value, "any", option_value_len) == 0) {
		kiwi_vars_set(&client->vars,
			      KIWI_VAR_ODYSSEY_TARGET_SESSION_ATTRS,
			      option_value, option_value_len);
	} else {
		/* some other option name, fallback to regular logic */
		return OD_OK;
	}

	od_debug(&instance->logger, "virtual processing", client, server,
		 "parsed tsa hint %.*s", option_value_len, option_value);

	/* skip this query */
	server->sync_point_deploy_msg =
		kiwi_be_write_command_complete(NULL, "SET", sizeof("SET"));
	if (server->sync_point_deploy_msg == NULL)
		return OD_ESERVER_WRITE;

	server->sync_point_deploy_msg =
		kiwi_be_write_ready(server->sync_point_deploy_msg, 'I');
	if (server->sync_point_deploy_msg == NULL)
		return OD_ESERVER_WRITE;

	/* request sync point to deploy our virtual responce to client */
	return OD_REQ_SYNC;
}

static od_frontend_status_t od_frontend_process_set_appname(od_client_t *client,
							    od_parser_t *parser)
{
	int rc;
	od_token_t token;

	rc = od_parser_next(parser, &token);
	switch (rc) {
	/* set application_name to ... */
	case OD_PARSER_KEYWORD: {
		od_keyword_t *keyword =
			od_keyword_match(od_query_process_keywords, &token);
		if (keyword == NULL || keyword->id != OD_QUERY_PROCESSING_LTO) {
			goto error;
		}
		break;
	}

	/* set application_name = ... */
	case OD_PARSER_SYMBOL:
		if (token.value.num != '=') {
			goto error;
		}
		break;

	default:
		goto error;
	}

	/* read original appname */
	rc = od_parser_next(parser, &token);
	if (rc != OD_PARSER_STRING) {
		goto error;
	}

	char original_appname[64];
	size_t len =
		(size_t)token.value.string.size > sizeof(original_appname) ?
			sizeof(original_appname) :
			(size_t)token.value.string.size;
	strncpy(original_appname, token.value.string.pointer, len);

	/* query should end with ; */
	rc = od_parser_next(parser, &token);
	if (rc != OD_PARSER_SYMBOL || token.value.num != ';') {
		goto error;
	}

	rc = od_parser_next(parser, &token);
	if (rc != OD_PARSER_EOF) {
		goto error;
	}

	char peer_name[KIWI_MAX_VAR_SIZE];
	rc = od_getpeername(client->io.io, peer_name, sizeof(peer_name), 1, 0);
	if (rc != 0) {
		od_gerror("query", client, client->server,
			  "can't get peer name, errno = ", machine_errno());
		goto error;
	}

	char suffix[KIWI_MAX_VAR_SIZE];
	od_snprintf(suffix, sizeof(suffix), " - %s", peer_name);

	char appname[64];
	int appname_len = od_concat_prefer_right(appname, sizeof(appname),
						 original_appname, len, suffix,
						 strlen(suffix));

	char query[128];
	od_snprintf(query, sizeof(query), "set application_name to '%.*s';",
		    appname_len, appname);

	machine_msg_t *msg;
	msg = kiwi_fe_write_query(NULL, query, strlen(query) + 1);
	if (msg == NULL) {
		od_gerror("query", client, client->server,
			  "can't create message to send \"%s\"", query);
		return OD_ESERVER_WRITE;
	}

	/* TODO: use iov add here */
	rc = od_write(&client->server->io, msg);
	if (rc != 0) {
		od_gerror("query", client, client->server,
			  "can't write \"%s\", rc = %d, errno = %d", query, rc,
			  machine_errno());
		return OD_ESERVER_WRITE;
	}

	od_server_sync_request(client->server, 1);

	return OD_SKIP;

error:
	/* can't handle, let pg do plain version of the query */
	return OD_OK;
}

static od_frontend_status_t od_frontend_process_set(od_client_t *client,
						    od_parser_t *parser)
{
	od_instance_t *instance = od_global_get_instance();
	int rc;
	od_token_t token;
	od_keyword_t *keyword;

	/* need to read attribute name that are setting */
	rc = od_parser_next(parser, &token);
	switch (rc) {
	case OD_PARSER_KEYWORD:
		keyword = od_keyword_match(od_query_process_keywords, &token);
		if (keyword == NULL) {
			/* some other option, skip */
			return OD_OK;
		}

		if (keyword->id == OD_QUERY_PROCESSING_LAPPNAME) {
			/* this is set application_name ... query */
			if (client->rule->application_name_add_host) {
				return od_frontend_process_set_appname(client,
								       parser);
			}
		}

		if (keyword->id == OD_QUERY_PROCESSING_LODYSSEY) {
			/*
			* this is odyssey-specific virtual values set like
			* set odyssey.target_session_attr = 'read-only';
			*/
			if (instance->config.virtual_processing) {
				int retstatus =
					od_process_virtual_set(client, parser);
				if (retstatus != OD_OK) {
					return retstatus;
				}
			}
		}
		break;
	default:
		return OD_OK;
	}

	return OD_OK;
}

static od_frontend_status_t
od_frontend_process_query(od_client_t *client, char *query, uint32_t query_len)
{
	od_instance_t *instance = od_global_get_instance();

	int need_process = client->rule->application_name_add_host ||
			   instance->config.virtual_processing;
	if (!need_process) {
		return OD_OK;
	}

	int rc;
	od_parser_t parser;
	od_parser_init_queries_mode(&parser, query,
				    query_len - 1 /* len is zero included */);

	od_token_t token;
	rc = od_parser_next(&parser, &token);

	/* all processed queries starts with show or set now */
	if (rc != OD_PARSER_KEYWORD) {
		return OD_OK;
	}

	od_keyword_t *keyword;
	keyword = od_keyword_match(od_query_process_keywords, &token);

	if (keyword == NULL) {
		return OD_OK;
	}

	switch (keyword->id) {
	case OD_QUERY_PROCESSING_LSET:
		return od_frontend_process_set(client, &parser);
	case OD_QUERY_PROCESSING_LSHOW:
	/* fallthrough */
	/* XXX: implement virtual show */
	default:
		return OD_OK;
	}
}

static od_frontend_status_t od_frontend_remote_client(od_relay_t *relay,
						      char *data, int size)
{
	uint32_t query_len;
	char *query;
	int rc;
	od_client_t *client = relay->client;
	od_instance_t *instance = client->global->instance;
	(void)size;
	od_route_t *route = client->route;
	assert(route != NULL);

	kiwi_fe_type_t type = *data;
	if (type == KIWI_FE_TERMINATE)
		return OD_STOP;

	/* get server connection from the route pool and write
	   configuration */
	od_server_t *server = client->server;
	assert(server != NULL);
	assert(server->parse_msg == NULL);

	/* XXX: reset query state on transaction block bound here.  */
	switch (type) {
	case KIWI_FE_SYNC:
	case KIWI_FE_QUERY:
		server->bind_failed = 0;
		break;
	default:
		break;
	}

	if (instance->config.log_debug)
		od_debug(&instance->logger, "remote client", client, server,
			 "%s", kiwi_fe_type_to_string(type));

	od_frontend_status_t retstatus = OD_OK;
	switch (type) {
	case KIWI_FE_COPY_DONE:
	case KIWI_FE_COPY_FAIL:
		/* client finished copy */
		server->done_fail_response_received++;
		break;
	case KIWI_FE_QUERY:
		rc = kiwi_be_read_query(data, size, &query, &query_len);

		if (rc != OK_RESPONSE) {
			return OD_ESERVER_WRITE;
		}

		if (instance->config.log_query || route->rule->log_query)
			od_log(&instance->logger, "query", client, NULL, "%.*s",
			       query_len, query);

		if (query_len >= 7 &&
		    route->rule->pool->reserve_prepared_statement) {
			if (strncmp(query, "DISCARD", 7) == 0) {
				od_debug(&instance->logger, "simple query",
					 client, server,
					 "discard detected, invalidate caches");
				od_hashmap_empty(server->prep_stmts);
			}
		}

		retstatus = od_frontend_process_query(client, query, query_len);
		if (retstatus != OD_OK) {
			return retstatus;
		}

		/* update server sync state */
		od_server_sync_request(server, 1);
		break;
	case KIWI_FE_FUNCTION_CALL:
	case KIWI_FE_SYNC:
		/* update server sync state */
		od_server_sync_request(server, 1);
		break;
	case KIWI_FE_DESCRIBE:
		if (instance->config.log_query || route->rule->log_query)
			od_frontend_log_describe(instance, client, data, size);

		/* we did not re-transmit faield bind to server, so we ignore this bind */
		if (server->bind_failed) {
			retstatus = OD_SKIP;
			/* do not reset server->bind_failed yet */
		} else if (route->rule->pool->reserve_prepared_statement) {
			uint32_t operator_name_len;
			char *operator_name;
			int rc;
			kiwi_fe_describe_type_t type;
			rc = kiwi_be_read_describe(data, size, &operator_name,
						   &operator_name_len, &type);
			if (rc == -1) {
				return OD_ECLIENT_READ;
			}
			if (type == KIWI_FE_DESCRIBE_PORTAL) {
				break; // skip this, we only need to rewrite statement
			}

			assert(client->prep_stmt_ids);
			retstatus = OD_SKIP;

			od_hashmap_elt_t key;
			key.len = operator_name_len;
			key.data = operator_name;

			od_hash_t keyhash = od_murmur_hash(key.data, key.len);
			od_hashmap_elt_t *desc = od_hashmap_find(
				client->prep_stmt_ids, keyhash, &key);

			if (desc == NULL) {
				od_debug(
					&instance->logger, "remote client",
					client, server,
					"%.*s (len %d) (%u) operator was not prepared by this client",
					operator_name_len, operator_name,
					operator_name_len, keyhash);
				return OD_ESERVER_WRITE;
			}

			od_hash_t body_hash =
				od_murmur_hash(desc->data, desc->len);
			char opname[OD_HASH_LEN];
			od_snprintf(opname, OD_HASH_LEN, "%08x", body_hash);

			/* fill internals structs in, send parse if needed */
			if (od_frontend_deploy_prepared_stmt(
				    server, &server->relay, "parse before bind",
				    desc->data, desc->len, body_hash, opname,
				    OD_HASH_LEN) != OD_OK) {
				return OD_ESERVER_WRITE;
			}

			machine_msg_t *msg;
			msg = kiwi_fe_write_describe(NULL, 'S', opname,
						     OD_HASH_LEN);

			if (msg == NULL) {
				return OD_ESERVER_WRITE;
			}

			if (instance->config.log_query ||
			    route->rule->log_query) {
				od_frontend_log_describe(instance, client,
							 machine_msg_data(msg),
							 machine_msg_size(msg));
			}

			// msg if deallocated automatically
			machine_iov_add(relay->iov, msg);
			od_dbg_printf_on_dvl_lvl(
				1, "client relay %p advance msg %c\n", relay,
				*(char *)machine_msg_data(msg));
		}
		break;
	case KIWI_FE_PARSE:
		if (instance->config.log_query || route->rule->log_query)
			od_frontend_log_parse(instance, client, "parse", data,
					      size);
		if (route->rule->pool->reserve_prepared_statement) {
			/* skip client parse msg */
			retstatus = OD_REQ_SYNC;
			kiwi_prepared_statement_t desc;
			int rc;
			rc = kiwi_be_read_parse_dest(data, size, &desc);
			if (rc) {
				return OD_ECLIENT_READ;
			}

			od_hash_t keyhash = od_murmur_hash(
				desc.operator_name, desc.operator_name_len);
			od_debug(&instance->logger, "parse", client, server,
				 "saving %.*s operator hash %u",
				 desc.operator_name_len, desc.operator_name,
				 keyhash);

			od_hashmap_elt_t key;
			key.len = desc.operator_name_len;
			key.data = desc.operator_name;

			od_hashmap_elt_t value;
			value.len = desc.description_len;
			value.data = desc.description;

			od_hashmap_elt_t *value_ptr = &value;

			server->parse_msg =
				machine_msg_create(desc.description_len);
			if (server->parse_msg == NULL) {
				return OD_ESERVER_WRITE;
			}
			memcpy(machine_msg_data(server->parse_msg),
			       desc.description, desc.description_len);

			assert(client->prep_stmt_ids);
			if (od_hashmap_insert(client->prep_stmt_ids, keyhash,
					      &key, &value_ptr)) {
				if (value_ptr->len != desc.description_len ||
				    strncmp(desc.description, value_ptr->data,
					    value_ptr->len) != 0) {
					/*
					* Raise error:
					* client allocated prepared stmt with same name
					*/
					return OD_ESERVER_WRITE;
				}
			}

			server->sync_point_deploy_msg =
				kiwi_be_write_parse_complete(NULL);
			if (server->sync_point_deploy_msg == NULL) {
				return OD_ESERVER_WRITE;
			}
		}
		break;
	case KIWI_FE_BIND:
		if (instance->config.log_query || route->rule->log_query)
			od_frontend_log_bind(instance, client, "bind", data,
					     size);

		if (route->rule->pool->reserve_prepared_statement) {
			retstatus = OD_SKIP;
			uint32_t operator_name_len;
			char *operator_name;

			int rc;
			rc = kiwi_be_read_bind_stmt_name(
				data, size, &operator_name, &operator_name_len);

			if (rc == -1) {
				return OD_ECLIENT_READ;
			}

			od_hashmap_elt_t key;
			key.len = operator_name_len;
			key.data = operator_name;

			od_hash_t keyhash = od_murmur_hash(key.data, key.len);

			od_hashmap_elt_t *desc =
				(od_hashmap_elt_t *)od_hashmap_find(
					client->prep_stmt_ids, keyhash, &key);
			if (desc == NULL) {
				char errbuf[OD_QRY_MAX_SZ];
				int errlen;

				od_debug(
					&instance->logger, "remote client",
					client, server,
					"%.*s (%u) operator was not prepared by this client",
					operator_name_len, operator_name,
					keyhash);

				assert(server->sync_point_deploy_msg == NULL);

				errlen = od_snprintf(
					errbuf, sizeof(errbuf),
					"prepared statement \"%.*s\" does not exist",
					operator_name_len, operator_name);

				server->sync_point_deploy_msg =
					kiwi_be_write_error(
						server->sync_point_deploy_msg,
						KIWI_UNDEFINED_PSTATEMENT,
						errbuf, errlen);
				server->bind_failed = 1;
				return OD_REQ_SYNC;
			}

			od_hash_t body_hash =
				od_murmur_hash(desc->data, desc->len);

			int invalidate = 0;

			if (desc->len >= 7) {
				if (strncmp(desc->data, "DISCARD", 7) == 0) {
					od_debug(
						&instance->logger,
						"rewrite bind", client, server,
						"discard detected, invalidate caches");
					invalidate = 1;
				}
			}

			char opname[OD_HASH_LEN];
			od_snprintf(opname, OD_HASH_LEN, "%08x", body_hash);

			/* fill internals structs in, send parse if needed */
			if (od_frontend_deploy_prepared_stmt(
				    server, &server->relay, "parse before bind",
				    desc->data, desc->len, body_hash, opname,
				    OD_HASH_LEN) != OD_OK) {
				return OD_ESERVER_WRITE;
			}

			int opname_start_offset =
				kiwi_be_bind_opname_offset(data, size);
			if (opname_start_offset < 0) {
				return OD_ECLIENT_READ;
			}

			machine_msg_t *msg;
			if (invalidate) {
				od_hashmap_empty(server->prep_stmts);
			}

			msg = od_frontend_rewrite_msg(data, size,
						      opname_start_offset,
						      operator_name_len, opname,
						      OD_HASH_LEN);

			if (msg == NULL) {
				return OD_ESERVER_WRITE;
			}

			if (instance->config.log_query ||
			    route->rule->log_query) {
				od_frontend_log_bind(instance, client,
						     "rewrite bind",
						     machine_msg_data(msg),
						     machine_msg_size(msg));
			}

			machine_iov_add(relay->iov, msg);

			od_dbg_printf_on_dvl_lvl(
				1, "client relay %p advance msg %c\n", relay,
				*(char *)machine_msg_data(msg));
		}
		break;
	case KIWI_FE_EXECUTE:
		if (instance->config.log_query || route->rule->log_query)
			od_frontend_log_execute(instance, client, data, size);
		if (server->bind_failed) {
			retstatus = OD_SKIP;
			/* do not reset server->bind_failed yet */
		}
		break;
	case KIWI_FE_CLOSE:
		if (route->rule->pool->reserve_prepared_statement) {
			char *name;
			uint32_t name_len;
			kiwi_fe_close_type_t type;
			int rc;

			if (od_frontend_parse_close(data, size, &name,
						    &name_len,
						    &type) != OK_RESPONSE) {
				return OD_ESERVER_WRITE;
			}

			if (type == KIWI_FE_CLOSE_PREPARED_STATEMENT) {
				retstatus = OD_SKIP;
				od_debug(
					&instance->logger,
					"ignore closing prepared statement, report its closed",
					client, server, "statement: %.*s",
					name_len, name);

				machine_msg_t *pmsg;
				pmsg = kiwi_be_write_close_complete(NULL);

				rc = od_write(&client->io, pmsg);
				if (rc == -1) {
					od_error(&instance->logger,
						 "close report", NULL, server,
						 "write error: %s",
						 od_io_error(&server->io));
					return OD_ECLIENT_WRITE;
				}
			}

			if (instance->config.log_query ||
			    route->rule->log_query) {
				od_frontend_log_close(instance, client, name,
						      name_len, type);
			}

		} else if (instance->config.log_query ||
			   route->rule->log_query) {
			char *name;
			uint32_t name_len;
			kiwi_fe_close_type_t type;

			if (od_frontend_parse_close(data, size, &name,
						    &name_len,
						    &type) != OK_RESPONSE) {
				return OD_ESERVER_WRITE;
			}

			od_frontend_log_close(instance, client, name, name_len,
					      type);
		}
		break;
	default:
		break;
	}

	/* If the retstatus is not SKIP */
	/* update server stats */
	od_stat_query_start(&server->stats_state);
	return retstatus;
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

/*
* machine_sleep with ODYSSEY_CATCHUP_RECHECK_INTERVAL value
* will be effitiently just a context switch.
*/

#define ODYSSEY_CATCHUP_RECHECK_INTERVAL 1

static inline od_frontend_status_t od_frontend_poll_catchup(od_client_t *client,
							    od_route_t *route,
							    uint32_t timeout)
{
	od_instance_t *instance = client->global->instance;

	od_dbg_printf_on_dvl_lvl(
		1, "client %s polling replica for catchup with timeout %d\n",
		client->id.id, timeout);

	/*
	 * Ensure heartbeet is initialized at least once.
	 * Heartbeat might be 0 after reload\restart.
	 */
	int absent_heartbeat_checks = 0;
	while (route->last_heartbeat == 0) {
		machine_sleep(ODYSSEY_CATCHUP_RECHECK_INTERVAL);
		/* add cast to int64_t for correct camparison 
  			(int64_t > int and int64_t > uint32_t) */
		if ((int64_t)absent_heartbeat_checks++ >
		    (timeout * (int64_t)1000 /
		     ODYSSEY_CATCHUP_RECHECK_INTERVAL)) {
			od_debug(&instance->logger, "catchup", client, NULL,
				 "No heartbeat for route detected\n");
			return OD_ECATCHUP_TIMEOUT;
		}
	}

	for (int check = 1; check <= route->rule->catchup_checks; ++check) {
		od_dbg_printf_on_dvl_lvl(1, "current cached time %d\n",
					 machine_timeofday_sec());
		int lag = machine_timeofday_sec() - route->last_heartbeat;
		if (lag < 0) {
			lag = 0;
		}
		if ((uint32_t)lag < timeout) {
			return OD_OK;
		}
		od_debug(
			&instance->logger, "catchup", client, NULL,
			"client %s replication %d lag is over catchup timeout %d\n",
			client->id.id, lag, timeout);
		/*
		 * TBD: Consider configuring `ODYSSEY_CATCHUP_RECHECK_INTERVAL` in
		 * frontend rule.
		 */
		if (check < route->rule->catchup_checks) {
			machine_sleep(ODYSSEY_CATCHUP_RECHECK_INTERVAL);
		}
	}
	return OD_ECATCHUP_TIMEOUT;
}

static inline od_frontend_status_t
od_frontend_remote_process_server(od_client_t *client, bool await_read)
{
	od_server_t *server = client->server;
	assert(server != NULL);
	od_frontend_status_t status = od_relay_step(&server->relay, await_read);
	int rc;
	od_instance_t *instance = client->global->instance;

	if (status == OD_DETACH) {
		/* detach on transaction or statement pooling  */
		/* write any pending data to server first */
		status = od_relay_flush(&server->relay);
		if (status != OD_OK)
			return status;

		od_relay_detach(&client->relay);
		od_relay_stop(&server->relay);

		/* cleanup server */
		rc = od_reset(server);
		if (rc != 1) {
			return OD_ESERVER_WRITE;
		}

		od_debug(&instance->logger, "detach", client, server,
			 "client %s%.*s detached from %s%.*s",
			 client->id.id_prefix,
			 (int)sizeof(client->id.id_prefix), client->id.id,
			 server->id.id_prefix,
			 (int)sizeof(server->id.id_prefix), server->id.id);

		/* push server connection back to route pool */
		od_router_t *router = client->global->router;
		od_router_detach(router, client);
		server = NULL;
	} else if (status != OD_OK) {
		return status;
	}
	return OD_OK;
}

static od_frontend_status_t
od_frontend_check_replica_catchup(od_instance_t *instance, od_client_t *client)
{
	od_route_t *route = client->route;

	assert(route);

	uint32_t catchup_timeout = route->rule->catchup_timeout;
	kiwi_var_t *timeout_var =
		kiwi_vars_get(&client->vars, KIWI_VAR_ODYSSEY_CATCHUP_TIMEOUT);
	od_frontend_status_t status = OD_OK;

	if (timeout_var != NULL) {
		/* if there is catchup pgoption variable in startup packet */
		char *end;
		uint32_t user_catchup_timeout =
			strtol(timeout_var->value, &end, 10);
		if (end == timeout_var->value + timeout_var->value_len) {
			// if where is no junk after number, thats ok
			catchup_timeout = user_catchup_timeout;
		} else {
			od_error(&instance->logger, "catchup", client, NULL,
				 "junk after catchup timeout, ignore value");
		}
	}

	if (catchup_timeout) {
		od_debug(&instance->logger, "catchup", client, NULL,
			 "checking for lag before doing any actual work");
		status = od_frontend_poll_catchup(client, route,
						  catchup_timeout);
	}

	return status;
}

static int wait_client_activity(od_client_t *client)
{
	/* io_cond is set up by client or server relay */
	if (machine_cond_wait(client->io_cond, 10 * 1000 /* 10 sec */) == 0) {
		client->time_last_active = machine_time_us();
		od_dbg_printf_on_dvl_lvl(
			1, "change client last active time %lld\n",
			client->time_last_active);

		return 1;
	}

	return 0;
}

static int wait_resume_transactional_step(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;
	od_server_t *server = client->server;

	/* client must be detached to start waiting for resume */
	if (server != NULL) {
		return 0;
	}

	int rc = od_global_wait_resumed(client->global, 1000 /* 1 sec */);
	if (rc == 0) {
		od_log(&instance->logger, "pause", client, server,
		       "waiting for global resume finished");
	} else {
		od_log(&instance->logger, "pause", client, server,
		       "client is waiting for global resume...");
	}

	return rc;
}

static int wait_resume_step(od_client_t *client)
{
	od_route_t *route = client->route;

	/* do not wait resume for console clients */
	if (route->rule->storage->storage_type == OD_RULE_STORAGE_LOCAL) {
		return 0;
	}

	if (!od_global_is_paused(client->global)) {
		return 0;
	}

	switch (route->rule->pool->pool_type) {
	case OD_RULE_POOL_SESSION:
		return 0;
	case OD_RULE_POOL_STATEMENT:
		/* fall through */
	case OD_RULE_POOL_TRANSACTION:
		return wait_resume_transactional_step(client);
	default:
		abort();
	}
}

/*
 * Wait some read/write events or connection drop condition to become true
 */
static od_frontend_status_t wait_any_activity(od_client_t *client)
{
	od_frontend_status_t status;

	for (;;) {
		status = od_process_connection_drop(client);
		if (status != OD_OK) {
			/* Odyssey is going to shut down or client conn is dropped
			* due some idle timeout, we drop the connection  */
			return status;
		}

#if OD_DEVEL_LVL != OD_RELEASE_MODE
		od_server_t *server = client->server;
		if (server != NULL && server->is_transaction &&
		    od_server_synchronized(server)) {
			od_dbg_printf_on_dvl_lvl(
				1, "here we have idle in transaction: cid %s\n",
				client->id.id);
		}
#endif

		if (wait_resume_step(client)) {
			/* need to wait for resume, but also must check for conn drop */
			continue;
		}

		if (wait_client_activity(client)) {
			break;
		}
	}

	return OD_OK;
}

static od_frontend_status_t od_frontend_remote(od_client_t *client)
{
	od_route_t *route = client->route;
	client->io_cond = machine_cond_create();

	if (client->io_cond == NULL) {
		return OD_EOOM;
	}

	od_frontend_status_t status;

	status = od_relay_start(client, &client->relay, OD_ECLIENT_READ,
				OD_ESERVER_WRITE,
				od_frontend_remote_client_on_read,
				&route->stats, od_frontend_remote_client);

	if (status != OD_OK) {
		return status;
	}

	od_server_t *server = NULL;
	od_instance_t *instance = client->global->instance;

	/* Consult lag polling logic and immudiately close connection with
	* error, if lag polling policy says so.
	*/

	od_frontend_status_t catchup_status =
		od_frontend_check_replica_catchup(instance, client);
	if (od_frontend_status_is_err(catchup_status)) {
		return catchup_status;
	}

	for (;;) {
		/* do not rewrite status if it wasn't OD_OK */
		od_frontend_status_t wait_status = wait_any_activity(client);
		if (wait_status != OD_OK) {
			status = wait_status;
		}

		if (od_frontend_status_is_err(status) || status == OD_STOP)
			break;

		/* Check for replication lag and reject query if too big */
		od_frontend_status_t catchup_status =
			od_frontend_check_replica_catchup(instance, client);
		if (od_frontend_status_is_err(catchup_status)) {
			status = catchup_status;
			break;
		}

		server = client->server;
		bool sync_req = 0;

		/* attach */
		status = od_relay_step(&client->relay, false);
		if (status == OD_ATTACH) {
			assert(server == NULL);
			status = od_frontend_attach_and_deploy(client, "main");
			if (status != OD_OK)
				break;
			server = client->server;
			status = od_relay_start(
				client, &server->relay, OD_ESERVER_READ,
				OD_ECLIENT_WRITE,
				od_frontend_remote_server_on_read,
				&route->stats, od_frontend_remote_server);
			if (status != OD_OK)
				break;
			od_relay_attach(&client->relay, &server->io);
			od_relay_attach(&server->relay, &client->io);

			/* retry read operation after attach */
			continue;
		} else if (status == OD_REQ_SYNC) {
			sync_req = 1;
		} else if (status != OD_OK) {
			break;
		}

		if (server == NULL)
			continue;

		status = od_frontend_remote_process_server(client, false);
		if (status != OD_OK) {
			/* should not return this here */
			assert(status != OD_REQ_SYNC);
			break;
		}

		// are we requested to meet sync point?

		if (sync_req) {
			od_debug(&instance->logger, "sync-point", client,
				 server, "process, %d",
				 od_server_synchronized(server));

			for (;;) {
				if (od_server_synchronized(server)) {
					break;
				}
				// await here
				od_debug(&instance->logger, "sync-point-await",
					 client, server, "process await");
				status = od_frontend_remote_process_server(
					client, true);

				if (status != OD_OK) {
					break;
				}
			}

			if (status != OD_OK) {
				od_log(&instance->logger, "sync-point", client,
				       server, "failed to meet sync point");
				break;
			}

			/* If we have pending parse message, do deploy */
			if (server->parse_msg != NULL) {
				/* fill internals structs in */
				if (od_frontend_deploy_prepared_stmt_msg(
					    server, &server->relay,
					    "sync-point-deploy") != OD_OK) {
					status = OD_ESERVER_WRITE;
					break;
				}

				machine_msg_t *msg;
				msg = kiwi_fe_write_sync(NULL);
				if (msg == NULL) {
					status = OD_ESERVER_WRITE;
					break;
				}
				int rc;
				rc = od_write(&server->io, msg);
				if (rc == -1) {
					status = OD_ESERVER_WRITE;
					break;
				}

				od_server_sync_request(server, 1);
			}

			/* enter sync point mode */
			server->sync_point = 1;

			for (;;) {
				if (od_server_synchronized(server)) {
					break;
				}
				// await here

				od_debug(&instance->logger, "sync-point",
					 client, server, "process await");
				status = od_frontend_remote_process_server(
					client, true);

				if (status != OD_OK) {
					break;
				}
			}

			server->sync_point = 0;
			if (status != OD_OK) {
				break;
			}

			if (server->sync_point_deploy_msg != NULL) {
				machine_iov_add(server->relay.iov,
						server->sync_point_deploy_msg);
				server->sync_point_deploy_msg = NULL;

				/* same as server src */
				assert(server->relay.src == client->relay.dst);

				/* we enqueued message to servers relay, so notify it. */
				machine_cond_signal(server->relay.src->on_read);
				status = od_frontend_remote_process_server(
					client, true);

				if (status != OD_OK) {
					break;
				}
			}
		}
		if (status != OD_OK) {
			break;
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
		od_frontend_fatal(client, KIWI_CONNECTION_FAILURE,
				  "failed to get remote server connection");
		break;

	case OD_EATTACH_TOO_MANY_CONNECTIONS:
		assert(server == NULL);
		assert(client->route != NULL);
		od_frontend_fatal(
			client, KIWI_TOO_MANY_CONNECTIONS,
			"too many active clients for user (pool_size for "
			"user %s.%s reached %d)",
			client->startup.database.value,
			client->startup.user.value,
			client->rule != NULL ? client->rule->pool->size : -1);
		break;

	case OD_EATTACH_TARGET_SESSION_ATTRS_MISMATCH:
		assert(server == NULL);
		assert(client->route != NULL);

		od_target_session_attrs_t attrs = od_tsa_get_effective(client);

		od_frontend_fatal(
			client, KIWI_CONNECTION_FAILURE,
			"can't find suitable host for tsa '%s' of user %s.%s",
			od_target_session_attrs_to_str(attrs),
			client->startup.database.value,
			client->startup.user.value);
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
			od_frontend_fatal(
				client, KIWI_CONNECTION_FAILURE,
				"failed to connect to remote server %s%.*s",
				server->id.id_prefix,
				(int)sizeof(server->id.id), server->id.id);
		}
		/* close backend connection */
		od_router_close(router, client);
		break;
	case OD_ECATCHUP_TIMEOUT:
		/* close client connection and close server
			 * connection in case of server errors */
		od_log(&instance->logger, context, client, server,
		       "replication lag is too big, failed to wait replica for catchup: status %s",
		       od_frontend_status_to_str(status));
		od_frontend_fatal(
			client, KIWI_CONNECTION_FAILURE,
			"remote server read/write error: failed to wait replica for catchup");

		if (client->server != NULL) {
			od_router_close(router, client);
		}
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
				  "remote server read/write error %s%.*s: %s",
				  server->id.id_prefix,
				  (int)sizeof(server->id.id), server->id.id,
				  od_io_error(&server->io));
		/* close backend connection */
		od_router_close(router, client);
		break;
	case OD_UNDEF:
	case OD_SKIP:
	case OD_REQ_SYNC:
	case OD_ATTACH:
	/* fallthrough */
	case OD_DETACH:
	case OD_ESYNC_BROKEN:
		od_error(&instance->logger, context, client, server,
			 "unexpected error status %s (%d)",
			 od_frontend_status_to_str(status), (uint32_t)status);
		od_router_close(router, client);
		break;
	default:
		od_error(
			&instance->logger, context, client, server,
			"unexpected error status %s (%d), possible corruption, abort()",
			od_frontend_status_to_str(status), (uint32_t)status);
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
	od_global_t *global = client->global;
	od_instance_t *instance = global->instance;
	od_router_t *router = global->router;
	od_extension_t *extensions = global->extensions;
	od_module_t *modules = extensions->modules;

	/* log client connection */
	if (instance->config.log_session) {
		od_getpeername(client->io.io, client->peer,
			       OD_CLIENT_MAX_PEERLEN, 1, 1);
		od_log(&instance->logger, "startup", client, NULL,
		       "new client connection %s", client->peer);
	}

	/* attach client io to worker machine event loop */
	int rc;
	rc = od_io_attach(&client->io);
	if (rc == -1) {
		od_error(&instance->logger, "startup", client, NULL,
			 "failed to transfer client io");
		od_io_close(&client->io);
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
			od_cancel(client->global, cancel.storage,
				  cancel.address, &cancel.key, &cancel.id);
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

	if (od_likely(router_status == OD_ROUTER_OK)) {
		od_route_t *route = client->route;
		if (route->rule->application_name_add_host) {
			od_application_name_add_host(client);
		}

		//override clients pg options if configured
		rc = kiwi_vars_override(&client->vars, &route->rule->vars);
		if (rc == -1) {
			goto cleanup;
		}

		char peer[128];
		od_getpeername(client->io.io, peer, sizeof(peer), 1, 0);

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
		case OD_ROUTER_INSUFFICIENT_ACCESS:
			// disabling blind ldapsearch via odyssey error messages
			// to collect user account attributes
			od_error(
				&instance->logger, "startup", client, NULL,
				"route for '%s.%s' is not found by ldapsearch for '%s' client, closing",
				client->startup.database.value,
				client->startup.user.value, peer);
			od_frontend_fatal(
				client, KIWI_SYSTEM_ERROR,
				"ldap authentication failed for user \"%s\": insufficient access",
				client->startup.user.value);
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

	/* HBA check */
	rc = od_hba_process(client);

	char client_ip[64];
	od_getpeername(client->io.io, client_ip, sizeof(client_ip), 1, 0);

	/* client authentication */
	if (rc == OK_RESPONSE) {
		/* Check for replication lag and reject query if too big before auth */
		od_frontend_status_t catchup_status =
			od_frontend_check_replica_catchup(instance, client);
		if (od_frontend_status_is_err(catchup_status)) {
			od_error(
				&instance->logger, "catchup", client, NULL,
				"replication lag too big, connection rejected: %s %s",
				client->startup.database.value,
				client->startup.user.value);

			od_frontend_fatal(
				client,
				KIWI_INVALID_AUTHORIZATION_SPECIFICATION,
				"replication lag too big, connection rejected: %s %s",
				client->startup.database.value,
				client->startup.user.value);
			rc = NOT_OK_RESPONSE;
		} else {
			rc = od_auth_frontend(client);
			od_log(&instance->logger, "auth", client, NULL,
			       "ip '%s' user '%s.%s': host based authentication allowed",
			       client_ip, client->startup.database.value,
			       client->startup.user.value);
		}
	} else {
		od_error(
			&instance->logger, "auth", client, NULL,
			"ip '%s' user '%s.%s': host based authentication rejected",
			client_ip, client->startup.database.value,
			client->startup.user.value);

		od_frontend_error(client, KIWI_INVALID_PASSWORD,
				  "host based authentication rejected");
	}

	uint64_t used_memory;
	od_rule_storage_type_t storage_type =
		client->route->rule->storage->storage_type;
	if (storage_type == OD_RULE_STORAGE_REMOTE &&
	    od_global_is_in_soft_oom(global, &used_memory)) {
		od_frontend_fatal(client, KIWI_OUT_OF_MEMORY,
				  "soft out of memory");
		od_error(&instance->logger, "startup", client, NULL,
			 "drop connection due to soft oom (usage is %lu bytes)",
			 used_memory);
		od_frontend_close(client);
		return;
	}

	if (rc != OK_RESPONSE) {
		/* rc == -1
		 * here we ignore module retcode because auth already failed
		 * we just inform side modules that usr was trying to log in
		 */
		od_list_foreach(&modules->link, i)
		{
			od_module_t *module;
			module = od_container_of(i, od_module_t, link);
			module->auth_complete_cb(client, rc);
		}
		goto cleanup;
	}

	/* auth result callback */
	od_list_foreach(&modules->link, i)
	{
		od_module_t *module;
		module = od_container_of(i, od_module_t, link);
		rc = module->auth_complete_cb(client, rc);
		if (rc != OD_MODULE_CB_OK_RETCODE) {
			// user blocked from module callback
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
