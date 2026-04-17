
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>

#include <status.h>
#include <types.h>
#include <frontend.h>
#include <client.h>
#include <router.h>
#include <server.h>
#include <rules.h>
#include <global.h>
#include <instance.h>
#include <cron.h>
#include <thread_global.h>
#include <ejection.h>
#include <parser.h>
#include <stream.h>
#include <query_processing.h>
#include <module.h>
#include <cancel.h>
#include <auth.h>
#include <reset.h>
#include <hba.h>
#include <dns.h>
#include <backend.h>
#include <tls.h>
#include <console.h>
#include <compression.h>
#include <extension.h>
#include <deploy.h>
#include <router_cancel.h>
#include <server.h>
#include <debugprintf.h>

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
	msg = od_frontend_fatal_msg(client, NULL, code, "", "", fmt, args);
	va_end(args);
	if (msg == NULL) {
		return -1;
	}
	return od_write(&client->io, msg);
}

int od_frontend_fatal_detailed(od_client_t *client, const char *code,
			       const char *detail, const char *hint,
			       const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	machine_msg_t *msg;
	msg = od_frontend_fatal_msg(client, NULL, code, detail, hint, fmt,
				    args);
	va_end(args);
	if (msg == NULL) {
		return -1;
	}
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
	if (rc == -1) {
		return -1;
	}
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
	if (msg == NULL) {
		return -1;
	}
	return od_write(&client->io, msg);
}

static inline bool
od_frontend_error_is_too_many_connections(od_client_t *client)
{
	od_server_t *server = client->server;
	assert(server != NULL);
	if (server->error_connect == NULL) {
		return false;
	}
	kiwi_fe_error_t error;
	int rc;
	rc = kiwi_fe_read_error(machine_msg_data(server->error_connect),
				machine_msg_size(server->error_connect),
				&error);
	if (rc == -1) {
		return false;
	}
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
		if (msg == NULL) {
			goto error;
		}

		int rc = kiwi_be_read_startup(machine_msg_data(msg),
					      machine_msg_size(msg),
					      &client->startup, &client->vars);
		machine_msg_free(msg);
		if (rc == -1) {
			goto error;
		}

		if (!client->startup.unsupported_request) {
			break;
		}
		/* not supported 'N' */
		msg = machine_msg_create(sizeof(uint8_t));
		if (msg == NULL) {
			return -1;
		}
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
	if (rc == -1) {
		goto error;
	}

	if (!client->startup.is_ssl_request) {
		rc = od_compression_frontend_setup(
			client, client->config_listen, &instance->logger);
		if (rc == -1) {
			return -1;
		}
		return 0;
	}

	/* read startup-cancel message followed after ssl
	 * negotiation */
	assert(client->startup.is_ssl_request);
	msg = od_read_startup(&client->io,
			      client->config_listen->client_login_timeout);
	if (msg == NULL) {
		goto error;
	}
	rc = kiwi_be_read_startup(machine_msg_data(msg), machine_msg_size(msg),
				  &client->startup, &client->vars);
	machine_msg_free(msg);
	if (rc == -1) {
		od_gerror("startup", client, NULL,
			  "startup packet parse error");
		goto error;
	}

	rc = od_compression_frontend_setup(client, client->config_listen,
					   &instance->logger);
	if (rc == -1) {
		return -1;
	}

	return 0;

error:
	od_log(&instance->logger, "startup", client, NULL,
	       "startup packet read error, errno = %d (%s)", machine_errno(),
	       strerror(machine_errno()));
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

	if (status_is_recent && !status.alive) {
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
		if (server->io.io && server->route->rule->pool->attach_check) {
			int rc = od_io_poll(&server->io);
			if (rc != 0) {
				od_error(&instance->logger, context, client,
					 server,
					 "server fd poll failed, errno=%d (%s)",
					 machine_errno(),
					 strerror(machine_errno()));
			}

			if (!od_io_connected(&server->io)) {
				od_log(&instance->logger, context, client,
				       server,
				       "server disconnected (%s), close connection and retry attach",
				       od_io_error(&server->io));
				od_router_close(router, client);
				continue;
			}
		}
		od_debug(&instance->logger, context, client, server,
			 "client %s%.*s attached to %s%.*s",
			 client->id.id_prefix, (int)sizeof(client->id.id),
			 client->id.id, server->id.id_prefix,
			 (int)sizeof(server->id.id), server->id.id);

		assert(od_server_synchronized(server));

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

				od_storage_endpoint_status_set_dead(
					&endpoint->status);
				return OD_ESERVER_CONNECT;
			}
		}

		int rc = od_backend_startup_preallocated(server, route_params,
							 client);
		if (rc != OK_RESPONSE) {
			od_storage_endpoint_status_set_dead(&endpoint->status);
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

od_frontend_status_t od_frontend_attach(od_client_t *client, char *context,
					kiwi_params_t *route_params)
{
	od_instance_t *instance = client->global->instance;
	od_route_t *route = client->route;
	od_rule_storage_t *storage = route->rule->storage;

	od_target_session_attrs_t tsa = od_tsa_get_effective(client);

	od_endpoint_attach_candidate_t candidates[OD_STORAGE_MAX_ENDPOINTS];
	od_frontend_attach_init_candidates(instance, storage, candidates, tsa,
					   0 /* prefer localhost */);

	od_frontend_status_t status = OD_EATTACH;

	for (size_t i = 0; i < storage->endpoints_count; ++i) {
		od_storage_endpoint_t *endpoint = candidates[i].endpoint;

		char addr[256];
		od_address_to_str(&endpoint->address, addr, sizeof(addr) - 1);

		od_debug(&instance->logger, context, client, NULL,
			 "trying to attach to %s...", addr);

		if (candidates[i].priority >= 0) {
			/*
			 * if client is attached now - previous attach failed
			 * but the server is still in active state and attached to client
			 *
			 * so now need to detach the server from the client,
			 * servers stays attached to client in case of error
			 * to have an ability to perform error forwarding
			 *
			 * TODO: fix this way of forwarding the error
			 */
			if (client->server != NULL) {
				od_router_close(client->global->router, client);
			}

			status = od_frontend_attach_to_endpoint(
				client, context, route_params, endpoint, tsa);
		} else {
			/* connection attempt will fail anyway */
			status = OD_EATTACH_TARGET_SESSION_ATTRS_MISMATCH;
		}

		if (status == OD_OK) {
			return status;
		}

		od_debug(&instance->logger, context, client, NULL,
			 "attach to %s failed with status: %s", addr,
			 od_frontend_status_to_str(status));
	}

	return status;
}

od_frontend_status_t od_frontend_attach_and_deploy(od_client_t *client,
						   char *context)
{
	/* attach and maybe connect server */
	od_frontend_status_t status;
	status = od_frontend_attach(client, context, NULL);
	if (status != OD_OK) {
		return status;
	}

	/* configure server using client parameters */
	if (client->rule->maintain_params) {
		int rc;
		rc = od_deploy(client, context);
		if (rc == -1) {
			return OD_ESERVER_WRITE;
		}
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

		od_router_detach(router, client);

		/* There is possible race here, so we will discard our
		 * attempt if params are already set */
		rc = kiwi_params_lock_set_once(&route->params, &route_params);
		if (!rc) {
			kiwi_params_free(&route_params);
		}
	}

	od_debug(&instance->logger, "setup", client, NULL, "sending params:");

	/* send parameters set by client or cached by the route */
	kiwi_param_t *param = route->params.params.list;

	machine_msg_t *stream = machine_msg_create(0);
	if (stream == NULL) {
		return OD_EOOM;
	}

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
	if (rc == -1) {
		return OD_ECLIENT_WRITE;
	}

	return OD_OK;
}

static inline od_frontend_status_t od_frontend_setup(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;
	od_route_t *route = client->route;

	/* set parameters */
	od_frontend_status_t status;
	status = od_frontend_setup_params(client);
	if (status != OD_OK) {
		return status;
	}

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
	if (msg == NULL) {
		return OD_EOOM;
	}
	stream = msg;

	/* write ready message */
	msg = kiwi_be_write_ready(stream, 'I');
	if (msg == NULL) {
		machine_msg_free(stream);
		return OD_EOOM;
	}

	int rc;
	rc = od_write(&client->io, stream);
	if (rc == -1) {
		return OD_ECLIENT_WRITE;
	}

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

	if (stream == NULL) {
		goto error;
	}
	/* client parameters */
	machine_msg_t *msg;
	char data[128];
	int data_len;
	/* current version and build */
#ifdef ODYSSEY_VERSION_GIT
	data_len = od_snprintf(data, sizeof(data), "%s (git %s)",
			       ODYSSEY_VERSION_NUMBER, ODYSSEY_VERSION_GIT);
#else
	data_len =
		od_snprintf(data, sizeof(data), "%s", ODYSSEY_VERSION_NUMBER);
#endif
	msg = kiwi_be_write_parameter_status(stream, "server_version", 15, data,
					     data_len + 1);
	if (msg == NULL) {
		goto error;
	}
	msg = kiwi_be_write_parameter_status(stream, "server_encoding", 16,
					     "UTF-8", 6);
	if (msg == NULL) {
		goto error;
	}
	msg = kiwi_be_write_parameter_status(stream, "client_encoding", 16,
					     "UTF-8", 6);
	if (msg == NULL) {
		goto error;
	}
	msg = kiwi_be_write_parameter_status(stream, "DateStyle", 10, "ISO", 4);
	if (msg == NULL) {
		goto error;
	}
	msg = kiwi_be_write_parameter_status(stream, "TimeZone", 9, "GMT", 4);
	if (msg == NULL) {
		goto error;
	}
	/* ready message */
	msg = kiwi_be_write_ready(stream, 'I');
	if (msg == NULL) {
		goto error;
	}
	int rc;
	rc = od_write(&client->io, stream);
	if (rc == -1) {
		return OD_ECLIENT_WRITE;
	}
	return OD_OK;
error:
	if (stream) {
		machine_msg_free(stream);
	}
	return OD_EOOM;
}

static inline bool od_eject_conn_with_rate(od_client_t *client,
					   od_server_t *server,
					   od_instance_t *instance)
{
	od_config_t *config = &instance->config;

	if (!config->conn_drop_options.drop_enabled) {
		return false;
	}

	if (server == NULL &&
	    client->rule->pool->pool_type == OD_RULE_POOL_SESSION) {
		od_log(&instance->logger, "shutdown", client, server,
		       "drop client because it was never attached to server");
		return true;
	}
	od_thread_global **gl = od_thread_global_get();
	if (gl == NULL) {
		od_log(&instance->logger, "shutdown", client, server,
		       "drop client connection on graceful shutdown, unable to throttle (wid %d)",
		       (*gl)->wid);
		/* this is clearly something bad, TODO: handle properly */
		return true;
	}

	od_conn_eject_info *info = (*gl)->info;

	uint64_t now_ms = machine_time_ms();
	bool res = false;

	if (od_conn_eject_info_try(info, now_ms)) {
		res = true;

		od_log(&instance->logger, "shutdown", client, server,
		       "drop client connection on graceful shutdown");
	} else {
		od_debug(
			&instance->logger, "shutdown", client, server,
			"delay drop client connection on graceful shutdown, rate limited");
	}

	return res;
}

static inline bool od_eject_conn_with_timeout(od_client_t *client,
					      __attribute__((unused))
					      od_server_t *server,
					      uint64_t timeout)
{
	assert(server != NULL);

	if (client->time_last_active + timeout < machine_time_us()) {
		return true;
	}

	return false;
}

static od_frontend_status_t od_frontend_ctl(od_client_t *client)
{
	if (od_atomic_u64_of(&client->killed) == 1) {
		return OD_ECLIENT_KILLED;
	}

	return OD_OK;
}

static inline od_frontend_status_t
od_process_drop_on_restart(od_client_t *client)
{
	/* TODO:: drop no more than X connection per sec/min/whatever */
	od_instance_t *instance = client->global->instance;
	od_server_t *server = client->server;

	int64_t shut_worker_id = od_instance_get_shutdown_worker_id(instance);

	if (od_likely(shut_worker_id == INVALID_COROUTINE_ID)) {
		/* try to optimize likely path */
		return OD_OK;
	}

	if (od_unlikely(client->rule->storage->storage_type ==
			OD_RULE_STORAGE_LOCAL)) {
		/* local server is not very important (db like console, pgbouncer used for stats) */
		return OD_EGRACEFUL_SHUTDOWN;
	}

	if (od_unlikely(server == NULL)) {
		if (od_eject_conn_with_rate(client, server, instance)) {
			return OD_EGRACEFUL_SHUTDOWN;
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
			return OD_EGRACEFUL_SHUTDOWN;
		}
		return OD_OK;
	}

	return OD_OK;
}

static inline od_frontend_status_t
od_process_drop_on_idle_in_transaction(od_client_t *client, od_server_t *server)
{
	od_instance_t *instance = client->global->instance;

	/* the same as above but we are going to drop client inside transaction block */
	if (server != NULL && server->is_transaction &&
	    /*server is sync - that means client executed some stmts and got get result, and now just... do nothing */
	    od_server_synchronized(server)) {
		if (od_eject_conn_with_timeout(
			    client, server,
			    client->rule->pool->idle_in_transaction_timeout)) {
			od_log(&instance->logger, "shutdown", client, server,
			       "drop idle in transaction connection on due timeout %d sec",
			       client->rule->pool->idle_in_transaction_timeout);

			return OD_EIDLE_IN_TRANSACTION_TIMEOUT;
		}
	}

	return OD_OK;
}

static inline od_frontend_status_t
od_process_drop_transaction_pool(od_client_t *client)
{
	if (client->server != NULL &&
	    client->rule->pool->idle_in_transaction_timeout > 0) {
		od_frontend_status_t status =
			od_process_drop_on_idle_in_transaction(client,
							       client->server);
		if (status != OD_OK) {
			return status;
		}
	}

	return od_process_drop_on_restart(client);
}

static inline od_frontend_status_t
od_process_drop_on_client_idle_timeout(od_client_t *client, od_server_t *server)
{
	od_instance_t *instance = client->global->instance;

	/*
	 * as we do not unroute client in session pooling after transaction block etc
	 * we should consider this case separately
	 * general logic is: if client do nothing long enough we can assume this is just a stale connection
	 * but we need to ensure this connection was initialized etc
	 */
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

			return OD_EIDLE_TIMEOUT;
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

static inline bool od_frontend_should_detach(od_route_t *route,
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

static inline od_retcode_t od_frontend_log_describe(od_instance_t *instance,
						    od_client_t *client,
						    char *data, int size)
{
	uint32_t name_len;
	char *name;
	int rc;
	kiwi_fe_describe_type_t t;
	rc = kiwi_be_read_describe(data, size, &name, &name_len, &t);
	if (rc == -1) {
		return NOT_OK_RESPONSE;
	}

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
	if (rc == -1) {
		return NOT_OK_RESPONSE;
	}

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
	if (rc == -1) {
		return NOT_OK_RESPONSE;
	}
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
	if (rc == -1) {
		return NOT_OK_RESPONSE;
	}

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
	if (rc == -1) {
		return NOT_OK_RESPONSE;
	}

	od_log(&instance->logger, ctx, client, client->server, "bind %.*s",
	       name_len, name);
	return OK_RESPONSE;
}

/*
* machine_sleep with ODYSSEY_CATCHUP_RECHECK_INTERVAL value
* will be effitiently just a context switch.
*/

#define ODYSSEY_CATCHUP_RECHECK_INTERVAL 1

static inline int od_frontend_poll_catchup(od_client_t *client,
					   od_route_t *route, uint32_t timeout,
					   int *lag_out)
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

		*lag_out = lag;

		if ((uint32_t)lag < timeout) {
			return OD_OK;
		}
		od_debug(
			&instance->logger, "catchup", client, NULL,
			"client %s%.*s replication %d lag is over catchup timeout %d\n",
			client->id.id_prefix, OD_ID_LEN, client->id.id, lag,
			timeout);
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
			/* if where is no junk after number, thats ok */
			catchup_timeout = user_catchup_timeout;
		} else {
			od_error(&instance->logger, "catchup", client, NULL,
				 "junk after catchup timeout, ignore value");
		}
	}

	if (catchup_timeout) {
		od_debug(&instance->logger, "catchup", client, NULL,
			 "checking for lag before doing any actual work");
		status =
			od_frontend_poll_catchup(client, route, catchup_timeout,
						 &client->last_catchup_lag);
	}

	return status;
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

static od_frontend_status_t
client_read_full_msg(od_client_t *client, const kiwi_header_t *header,
		     size_t size, machine_msg_t **out, uint32_t timeout_ms)
{
	*out = NULL;

	machine_msg_t *msg;
	msg = machine_msg_create(sizeof(kiwi_header_t) + size);
	if (msg == NULL) {
		return OD_EOOM;
	}

	char *dest;
	dest = machine_msg_data(msg);
	memcpy(dest, header, sizeof(kiwi_header_t));
	dest += sizeof(kiwi_header_t);

	int rc = od_io_read(&client->io, dest, size, timeout_ms);
	if (rc == -1) {
		machine_msg_free(msg);
		return OD_ECLIENT_READ;
	}

	od_stat_t *stats = &client->route->stats;
	od_stat_recv_client(stats, size + sizeof(kiwi_header_t));

	*out = msg;
	return OD_OK;
}

static od_frontend_status_t client_process_message_full(od_client_t *client,
							machine_msg_t *msg,
							uint32_t timeout_ms)
{
	od_frontend_status_t status;
	int rc;
	od_instance_t *instance = client->global->instance;
	od_route_t *route = client->route;
	od_server_t *server = client->server;
	assert(route != NULL);

	char *data = machine_msg_data(msg);
	int size = machine_msg_size(msg);

	kiwi_fe_type_t type = *data;

	if (instance->config.log_debug) {
		od_debug(&instance->logger, "remote client", client, server,
			 "%s", kiwi_fe_type_to_string(type));
	}

	switch (type) {
	case KIWI_FE_TERMINATE:
		status = OD_STOP;
		break;
	case KIWI_FE_QUERY:
		if (instance->config.log_query || route->rule->log_query) {
			char *query;
			uint32_t query_len;
			rc = kiwi_be_read_query(data, size, &query, &query_len);
			if (rc == 0) {
				od_log(&instance->logger, "query", client,
				       client->server, "%.*s", query_len,
				       query);
			} else {
				od_error(&instance->logger, "query", client,
					 client->server, "invalid query msg");
			}
		}

		status =
			od_relay_process_query(&client->relay, msg, timeout_ms);
		break;
	case KIWI_FE_FUNCTION_CALL:
		status =
			od_relay_process_fcall(&client->relay, msg, timeout_ms);
		break;
	case KIWI_FE_FLUSH:
		status = od_relay_process_xflush(&client->relay, msg,
						 timeout_ms);
		break;
	case KIWI_FE_SYNC:
		status =
			od_relay_process_xsync(&client->relay, msg, timeout_ms);
		break;
	case KIWI_FE_PARSE:
		if (instance->config.log_query || route->rule->log_query) {
			od_frontend_log_parse(instance, client, "parse", data,
					      size);
		}
		status = od_relay_process_xmsg(&client->relay, msg, timeout_ms);
		break;
	case KIWI_FE_BIND:
		if (instance->config.log_query || route->rule->log_query) {
			od_frontend_log_bind(instance, client, "bind", data,
					     size);
		}
		status = od_relay_process_xmsg(&client->relay, msg, timeout_ms);
		break;
	case KIWI_FE_DESCRIBE:
		if (instance->config.log_query || route->rule->log_query) {
			od_frontend_log_describe(instance, client, data, size);
		}
		status = od_relay_process_xmsg(&client->relay, msg, timeout_ms);
		break;
	case KIWI_FE_EXECUTE:
		if (instance->config.log_query || route->rule->log_query) {
			od_frontend_log_execute(instance, client, data, size);
		}
		status = od_relay_process_xmsg(&client->relay, msg, timeout_ms);
		break;
	case KIWI_FE_CLOSE:
		if (instance->config.log_query || route->rule->log_query) {
			char *name;
			uint32_t name_len;
			kiwi_fe_close_type_t type;

			rc = od_frontend_parse_close(data, size, &name,
						     &name_len, &type);
			if (rc != OK_RESPONSE) {
				od_gerror("main", client, server,
					  "can't parse close message");
				machine_msg_free(msg);
				return OD_ESERVER_READ;
			}

			od_frontend_log_close(instance, client, name, name_len,
					      type);
		}
		status = od_relay_process_xmsg(&client->relay, msg, timeout_ms);
		break;

	case KIWI_FE_COPY_DONE:
	case KIWI_FE_COPY_FAIL:
	case KIWI_FE_COPY_DATA:
		/*
		 * copy are handled inside process query
		 * but after receiving the error on copy, client might
		 * send some copy messages, that must be just dropped
		 *
		 * so do nothing
		 */
		status = OD_OK;

		break;

	default:
		od_gerror("main", client, server,
			  "unexpected message type '%c' from client", type);
		return OD_ECLIENT_PROTOCOL_ERROR;
	}

	return status;
}

static od_frontend_status_t process_possible_detach(od_client_t *client)
{
	od_route_t *route = client->route;
	od_instance_t *instance = client->global->instance;
	od_server_t *server = client->server;

	if (server == NULL) {
		return OD_OK;
	}

	if (!od_server_synchronized(server)) {
		return OD_OK;
	}

	if (server->client_pinned) {
		return OD_OK;
	}

	if (!od_frontend_should_detach(route, server)) {
		return OD_OK;
	}

	/* should detach */

	/* cleanup server */
	int rc = od_reset(server);
	if (rc != 1) {
		return OD_ESERVER_WRITE;
	}

	od_debug(&instance->logger, "detach", client, server,
		 "client %s%.*s detached from %s%.*s", client->id.id_prefix,
		 (int)sizeof(client->id.id_prefix), client->id.id,
		 server->id.id_prefix, (int)sizeof(server->id.id_prefix),
		 server->id.id);

	/* push server connection back to route pool */
	od_router_t *router = client->global->router;
	od_router_detach(router, client);

	return OD_OK;
}

static od_frontend_status_t client_process_message(od_client_t *client,
						   kiwi_header_t *header,
						   uint32_t timeout_ms)
{
	od_frontend_status_t status;

	uint32_t size;
	int rc = kiwi_validate_header((char *)header, sizeof(kiwi_header_t),
				      &size);
	if (rc == -1) {
		od_gerror(
			"main", client, client->server,
			"client msg header validation failed (type=%d, size=%u)",
			header->type, header->len);
		return OD_ECLIENT_READ;
	}
	size -= sizeof(uint32_t);

	machine_msg_t *msg = NULL;
	status = client_read_full_msg(client, header, size, &msg, timeout_ms);
	if (status != OD_OK) {
		return status;
	}

	status = client_process_message_full(client, msg, timeout_ms);

	machine_msg_free(msg);

	if (status != OD_OK) {
		return status;
	}

	status = process_possible_detach(client);

	return status;
}

static od_frontend_status_t client_read_header(od_client_t *client,
					       kiwi_header_t *header)
{
	while (od_readahead_unread(&client->io.readahead) <
	       (int)sizeof(kiwi_header_t)) {
		int rc = od_io_read_some(&client->io, 1000);
		if (rc != 0) {
			if (machine_timedout()) {
				return OD_ECLIENT_TIMEOUT;
			}
			return OD_ECLIENT_READ;
		}
	}

	int rc = od_io_read(&client->io, (char *)header, sizeof(kiwi_header_t),
			    1000);
	if (rc != 0) {
		/*
		 * impossible - readahead already contains enough
		 * bytes to read header
		 */
		return OD_ECLIENT_READ;
	}

	return OD_OK;
}
typedef struct {
	od_client_t *client;
	od_server_t *server;
	int is_client_to_server;
	od_frontend_status_t retstatus;
	int errno_;
} replication_pipe_arg_t;

static void od_frontend_replication_pipe(void *arg_)
{
	replication_pipe_arg_t *arg = arg_;
	od_client_t *client = arg->client;
	od_server_t *server = arg->server;
	int is_client_to_server = arg->is_client_to_server;
	od_stat_t *stats = &client->route->stats;

	od_io_t *src = NULL;
	od_io_t *dst = NULL;
	od_frontend_status_t read_err = OD_UNDEF;
	od_frontend_status_t write_err = OD_UNDEF;
	od_readahead_t *readahead = NULL;
	od_frontend_status_t status;

	int rc, errno_ = 0;

	if (is_client_to_server) {
		src = &client->io;
		dst = &server->io;
		read_err = OD_ECLIENT_READ;
		write_err = OD_ESERVER_WRITE;
	} else {
		src = &server->io;
		dst = &client->io;
		read_err = OD_ESERVER_READ;
		write_err = OD_ECLIENT_WRITE;
	}

	readahead = &src->readahead;

	while (1) {
		status = od_frontend_ctl(client);
		if (status != OD_OK) {
			break;
		}

		status = od_process_drop_on_restart(client);
		if (status != OD_OK) {
			break;
		}

		struct iovec rvec = od_readahead_read_begin(readahead);
		if (rvec.iov_len == 0) {
			rc = od_io_read_some(src, 1000);
			errno_ = machine_errno();
			if (rc == 0) {
				if (is_client_to_server) {
					od_stat_recv_client(
						stats,
						od_readahead_unread(readahead));
				} else {
					od_stat_recv_server(
						stats,
						od_readahead_unread(readahead));
				}
			} else if (errno_ != EAGAIN && errno_ != ETIMEDOUT) {
				status = read_err;
				break;
			}

			continue;
		}

		size_t written;
		rc = od_io_write_raw(dst, rvec.iov_base, rvec.iov_len, &written,
				     1000);
		errno_ = machine_errno();
		od_readahead_read_commit(readahead, written);
		if (rc < 0 && errno_ != EAGAIN && errno_ != ETIMEDOUT) {
			status = write_err;
			break;
		}
	}

	arg->retstatus = status;
	arg->errno_ = errno_;
}

static od_frontend_status_t od_frontend_remote_replication(od_client_t *client)
{
	/*
	 * we do not return replication backends to pool
	 * we do not want to have any features like catchup_timeout
	 * to work with replication connection
	 * we do not want to count any statistics for them
	 *
	 * so lets just create two pipe coroutines that just proxy
	 * all bytes for both directions
	 */

	od_frontend_status_t status;
	od_instance_t *instance = client->global->instance;

	if (instance->config.log_session) {
		od_log(&instance->logger, "main", client, NULL,
		       "replication connection");
	}

	if (client->server == NULL) {
		status = od_frontend_attach_and_deploy(client, "main");
		if (status != OD_OK) {
			return status;
		}
		assert(client->server != NULL);
	}

	replication_pipe_arg_t cl_srv_arg;
	memset(&cl_srv_arg, 0, sizeof(replication_pipe_arg_t));
	cl_srv_arg.client = client;
	cl_srv_arg.server = client->server;
	cl_srv_arg.is_client_to_server = 1;

	replication_pipe_arg_t srv_cl_arg;
	memset(&srv_cl_arg, 0, sizeof(replication_pipe_arg_t));
	srv_cl_arg.client = client;
	srv_cl_arg.server = client->server;
	srv_cl_arg.is_client_to_server = 0;

	char coro_name[OD_ID_LEN + 16 /* '>s' or '<s' */];
	memset(coro_name, 0, sizeof(coro_name));
	od_id_write_to_string(&client->id, coro_name, sizeof(coro_name));
	int idlen = strlen(coro_name);
	memcpy(coro_name + idlen, ">s", 2);
	int64_t cl_srv = machine_coroutine_create_named(
		od_frontend_replication_pipe, &cl_srv_arg, coro_name);
	if (cl_srv == -1) {
		od_error(
			&instance->logger, "main", client, client->server,
			"can't start client->server pipe coroutine, errno=%d (%s)",
			machine_errno(), strerror(machine_errno()));
		return OD_UNDEF;
	}

	memcpy(coro_name + idlen, "<s", 2);
	int64_t srv_cl = machine_coroutine_create_named(
		od_frontend_replication_pipe, &srv_cl_arg, coro_name);
	if (srv_cl == -1) {
		machine_cancel(cl_srv);
		machine_join(cl_srv);
		od_error(
			&instance->logger, "main", client, client->server,
			"can't start server->client pipe coroutine, errno=%d (%s)",
			machine_errno(), strerror(machine_errno()));
		return OD_UNDEF;
	}

	machine_join(srv_cl);
	machine_join(cl_srv);

	if (instance->config.log_session) {
		od_log(&instance->logger, "main", client, client->server,
		       "client->server replication finished with status %d (%s), errno=%d (%s)",
		       cl_srv_arg.retstatus,
		       od_frontend_status_to_str(cl_srv_arg.retstatus),
		       cl_srv_arg.errno_, strerror(cl_srv_arg.errno_));

		od_log(&instance->logger, "main", client, client->server,
		       "server->client replication finished with status %d (%s), errno=%d (%s)",
		       srv_cl_arg.retstatus,
		       od_frontend_status_to_str(srv_cl_arg.retstatus),
		       srv_cl_arg.errno_, strerror(srv_cl_arg.errno_));
	}

	if (cl_srv_arg.retstatus != OD_OK) {
		mm_errno_set(cl_srv_arg.errno_);
		return cl_srv_arg.retstatus;
	}

	if (srv_cl_arg.retstatus != OD_OK) {
		mm_errno_set(srv_cl_arg.errno_);
		return srv_cl_arg.retstatus;
	}

	return OD_OK;
}

static od_frontend_status_t od_frontend_remote(od_client_t *client)
{
	if (client->route->id.logical_rep || client->route->id.physical_rep) {
		return od_frontend_remote_replication(client);
	}

	od_instance_t *instance = client->global->instance;

	while (1) {
		od_frontend_status_t status =
			od_frontend_check_replica_catchup(instance, client);
		if (od_frontend_status_is_err(status)) {
			return status;
		}

		status = od_frontend_ctl(client);
		if (status != OD_OK) {
			return status;
		}

		status = od_process_connection_drop(client);
		if (status != OD_OK) {
			return status;
		}

		if (wait_resume_step(client)) {
			/* need to wait for resume, but also must check for conn drop */
			continue;
		}

		kiwi_header_t header;
		status = client_read_header(client, &header);
		if (status == OD_ECLIENT_TIMEOUT) {
			continue;
		}
		if (status != OD_OK) {
			return status;
		}

		status = client_process_message(client, &header,
						/* TODO */ UINT32_MAX);
		if (status != OD_OK) {
			return status;
		}

		client->time_last_active = machine_time_us();
	}
}

static void od_frontend_report_drop(od_client_t *client, char *context,
				    od_frontend_status_t status)
{
	od_server_t *server = client->server;
	od_instance_t *instance = client->global->instance;

	switch (status) {
	case OD_EIDLE_TIMEOUT:
		od_log(&instance->logger, context, client, server,
		       "client dropped by idle timeout");
		od_frontend_fatal_detailed(
			client, KIWI_CONNECTION_FAILURE,
			"Connection was dropped by Odyssey due to idle timeout",
			"", "Odyssey has dropped the connection");
		break;
	case OD_EIDLE_IN_TRANSACTION_TIMEOUT:
		od_log(&instance->logger, context, client, server,
		       "client dropped by idle in transaction timeout");
		od_frontend_fatal_detailed(
			client, KIWI_CONNECTION_FAILURE,
			"Connection was dropped by Odyssey due to idle in transaction timeout",
			"", "Odyssey has dropped the connection");
		break;
	case OD_ECLIENT_KILLED:
		od_log(&instance->logger, context, client, server,
		       "client killed by reload or console command");
		od_frontend_fatal_detailed(
			client, KIWI_CONNECTION_FAILURE,
			"Connection was killed by Odyssey configuration reloading or console command",
			"Try to reconnect",
			"Odyssey has dropped the connection");
		break;
	default:
		od_log(&instance->logger, context, client, server,
		       "unexpected reason for client drop: %d (%s)", status,
		       od_frontend_status_to_str(status));
		od_frontend_fatal_detailed(
			client, KIWI_CONNECTION_FAILURE,
			"Connection was dropped by unexpected reason", "",
			"Odyssey has dropped the connection");
		break;
	}
}

static void od_frontend_on_client_disconnect(od_frontend_status_t status,
					     od_client_t *client, char *context,
					     int force_server_close)
{
	int rc;
	od_instance_t *instance = client->global->instance;
	od_router_t *router = client->global->router;
	od_server_t *server = client->server;
	od_log(&instance->logger, context, client, server,
	       "client disconnected, addr '%s', io error = %s, status %s, working time %lldus",
	       client->peer, od_io_error(&client->io),
	       od_frontend_status_to_str(status),
	       machine_time_us() - client->time_accept);
	if (server == NULL) {
		return;
	}

	if (force_server_close) {
		od_router_close(router, client);
		return;
	}

	rc = od_reset(server);
	if (rc != 1) {
		/* close backend connection */
		od_log(&instance->logger, context, client, server,
		       "reset unsuccessful, closing server connection");
		od_router_close(router, client);
		return;
	}

	/* push server to router server pool */
	od_router_detach(router, client);
}

static void od_frontend_cleanup(od_client_t *client, char *context,
				od_frontend_status_t status,
				od_error_logger_t *l)
{
	od_instance_t *instance = client->global->instance;
	od_router_t *router = client->global->router;
	od_route_t *route = client->route;
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
	case OD_EIDLE_TIMEOUT:
		/* fallthrough */
	case OD_EIDLE_IN_TRANSACTION_TIMEOUT:
		/* fallthrough */
	case OD_ECLIENT_KILLED:
		od_frontend_report_drop(client, context, status);
		/* fallthrough */
	case OD_STOP:
	/* fallthrough */
	case OD_OK:
		/* graceful disconnect or kill */
		if (instance->config.log_session) {
			od_log(&instance->logger, context, client, server,
			       "client disconnected (route %s.%s, working time: %lldus)",
			       route->rule->db_name, route->rule->user_name,
			       machine_time_us() - client->time_accept);
		}
		if (!client->server) {
			break;
		}

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
		if (client->server) {
			od_router_close(router, client);
		}
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

	case OD_EGRACEFUL_SHUTDOWN:
		if (od_global_get_instance()->pid.restart_new_pid != -1) {
			od_frontend_fatal_detailed(
				client, KIWI_CONNECTION_FAILURE,
				"The Odyssey instance is performing online restart to update configuration or binary, and the connections are being drained",
				"Try to reconnect",
				"Odyssey is gracefully shutting down");
		} else {
			od_frontend_fatal_detailed(
				client, KIWI_CONNECTION_FAILURE,
				"The Odyssey instance is gracefully shutting down, and the connections are being drained",
				"", "Odyssey is gracefully shutting down");
		}
		/* fallthrough */
	case OD_ECLIENT_READ:
		/*fallthrough*/
	case OD_ECLIENT_WRITE:
		/* close client connection and reuse server
			 * link in case of client errors */
		od_frontend_on_client_disconnect(status, client, context,
						 0 /* force server close */);
		break;

	case OD_ECLIENT_COPY_IN_XPROTO:
		od_frontend_fatal_detailed(
			client, KIWI_SYSTEM_ERROR,
			"Odyssey met CopyInResponse when executing xproto messages, this is not implemented now",
			"Contact developers, see https://github.com/yandex/odyssey for more",
			"Copy protocol in xproto met, not implemented now");
		od_frontend_on_client_disconnect(status, client, context,
						 1 /* force server close */);
		break;

	case OD_ECLIENT_PROTOCOL_ERROR:
		od_frontend_fatal(
			client, KIWI_PROTOCOL_VIOLATION,
			"unexpected message type from client or other protocol violation");
		od_frontend_on_client_disconnect(status, client, context,
						 0 /* force server close */);
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
		od_frontend_on_client_disconnect(status, client, context,
						 1 /* force server close */);
		break;
	case OD_ECATCHUP_TIMEOUT:
		/* close client connection and close server
			 * connection in case of server errors */
		od_log(&instance->logger, context, client, server,
		       "replication lag is too big (%d sec), failed to wait replica for catchup: status %s",
		       client->last_catchup_lag,
		       od_frontend_status_to_str(status));

		od_frontend_fatal_detailed(
			client, KIWI_CONNECTION_FAILURE,
			"replication lag of the node you are trying to connect is too big, connection rejected",
			"wait until the replica catches up with the primary",
			"replication lag too big (%d seconds), connection rejected: %s %s",
			client->last_catchup_lag,
			client->startup.database.value,
			client->startup.user.value);

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
		od_frontend_on_client_disconnect(status, client, context,
						 1 /* force server close */);
		break;
	case OD_UNDEF:
	case OD_SKIP:
	case OD_REQ_SYNC:
	case OD_ATTACH:
	/* fallthrough */
	case OD_DETACH:
	case OD_ESYNC_BROKEN:
	default:
		od_error(&instance->logger, context, client, server,
			 "unexpected error status %s (%d)",
			 od_frontend_status_to_str(status), (uint32_t)status);
		od_router_close(router, client);
		break;
	}
}

static void od_application_name_add_host(od_client_t *client)
{
	if (client == NULL || client->io.io == NULL) {
		return;
	}
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
		       1); /* include both address and port */

	int length =
		od_snprintf(app_name_with_host, KIWI_MAX_VAR_SIZE, "%.*s - %s",
			    app_name_len, app_name, peer_name);
	kiwi_vars_set(&client->vars, KIWI_VAR_APPLICATION_NAME,
		      app_name_with_host, length + 1); /* return code ignored */
}

void od_frontend(void *arg)
{
	od_client_t *client = arg;
	od_global_t *global = client->global;
	od_instance_t *instance = global->instance;
	od_router_t *router = global->router;
	od_extension_t *extensions = global->extensions;
	od_module_t *modules = extensions->modules;

	od_getpeername(client->io.io, client->peer, OD_CLIENT_MAX_PEERLEN, 1,
		       1);

	/* log client connection */
	if (instance->config.log_session) {
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
			/*
			 * server might be free during cancel end
			 * so need to preserve it route ptr
			 */
			od_route_t *srv_route = cancel.server->route;

			od_cancel(client->global, cancel.storage,
				  cancel.address, &cancel.key, &cancel.id);

			od_route_lock(srv_route);
			od_server_cancel_end(cancel.server);
			od_route_unlock(srv_route);
			/* signal about possible free connection */
			od_route_signal(srv_route);

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

		/* override clients pg options if configured */
		rc = kiwi_vars_override(&client->vars, &route->rule->vars);
		if (rc == -1) {
			goto cleanup;
		}

		if (instance->config.log_session) {
			od_log(&instance->logger, "startup", client, NULL,
			       "route '%s.%s' to '%s.%s'",
			       client->startup.database.value,
			       client->startup.user.value, route->rule->db_name,
			       route->rule->user_name);
		}
	} else {
		if (od_router_status_is_err(router_status)) {
			od_error_logger_store_err(router->router_err_logger,
						  router_status);
		}

		switch (router_status) {
		case OD_ROUTER_ERROR:
			od_error(&instance->logger, "startup", client, NULL,
				 "routing failed for '%s' client, closing",
				 client->peer);
			od_frontend_error(client, KIWI_SYSTEM_ERROR,
					  "client routing failed");
			break;
		case OD_ROUTER_INSUFFICIENT_ACCESS:
			/*
			 * disabling blind ldapsearch via odyssey error messages
			 * to collect user account attributes
			 */
			od_error(
				&instance->logger, "startup", client, NULL,
				"route for '%s.%s' is not found by ldapsearch for '%s' client, closing",
				client->startup.database.value,
				client->startup.user.value, client->peer);
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
				client->startup.user.value, client->peer);
			od_frontend_error(client, KIWI_UNDEFINED_DATABASE,
					  "route for '%s.%s' is not found",
					  client->startup.database.value,
					  client->startup.user.value);
			break;
		case OD_ROUTER_ERROR_LIMIT:
			od_error(
				&instance->logger, "startup", client, NULL,
				"global connection limit reached for '%s' client, closing",
				client->peer);

			od_frontend_error(
				client, KIWI_TOO_MANY_CONNECTIONS,
				"too many client tcp connections (global client_max)");
			break;
		case OD_ROUTER_ERROR_LIMIT_ROUTE:
			od_error(
				&instance->logger, "startup", client, NULL,
				"route connection limit reached for client '%s', closing",
				client->peer);
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
				client->peer);

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
	od_list_foreach (&modules->link, i) {
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
				"replication lag too big (%d), connection rejected: %s %s",
				client->last_catchup_lag,
				client->startup.database.value,
				client->startup.user.value);

			od_frontend_fatal_detailed(
				client, KIWI_CONNECTION_FAILURE,
				"replication lag of the node you are trying to connect is too big, connection rejected",
				"wait until the replica catches up with the primary",
				"replication lag too big (%d seconds), connection rejected: %s %s",
				client->last_catchup_lag,
				client->startup.database.value,
				client->startup.user.value);
			rc = NOT_OK_RESPONSE;
		} else {
			rc = od_auth_frontend(client);

			if (instance->config.log_session) {
				od_log(&instance->logger, "auth", client, NULL,
				       "ip '%s' user '%s.%s': host based authentication allowed",
				       client_ip,
				       client->startup.database.value,
				       client->startup.user.value);
			}
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

	uint64_t used_memory = 0;
	od_rule_storage_type_t storage_type =
		client->route->rule->storage->storage_type;
	if (storage_type == OD_RULE_STORAGE_REMOTE &&
	    od_global_is_in_soft_oom(global, &used_memory)) {
		od_frontend_fatal(client, KIWI_OUT_OF_MEMORY,
				  "soft out of memory");
		od_error(&instance->logger, "startup", client, NULL,
			 "drop connection due to soft oom (usage is %lu KB)",
			 used_memory / 1024);
		rc = NOT_OK_RESPONSE;
	}

	if (rc != OK_RESPONSE) {
		/* rc == -1
		 * here we ignore module retcode because auth already failed
		 * we just inform side modules that usr was trying to log in
		 */
		od_list_foreach (&modules->link, i) {
			od_module_t *module;
			module = od_container_of(i, od_module_t, link);
			module->auth_complete_cb(client, rc);
		}
		goto cleanup;
	}

	/* auth result callback */
	od_list_foreach (&modules->link, i) {
		od_module_t *module;
		module = od_container_of(i, od_module_t, link);
		rc = module->auth_complete_cb(client, rc);
		if (rc != OD_MODULE_CB_OK_RETCODE) {
			/* user blocked from module callback */
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
		if (status != OD_OK) {
			break;
		}

		status = od_frontend_local(client);
		break;
	}
	case OD_RULE_STORAGE_REMOTE: {
		status = od_frontend_setup(client);
		if (status != OD_OK) {
			break;
		}

		status = od_frontend_remote(client);
		break;
	}
	}
	od_error_logger_t *l;
	l = router->route_pool.err_logger;

	od_frontend_cleanup(client, "main", status, l);

	od_list_foreach (&modules->link, i) {
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
