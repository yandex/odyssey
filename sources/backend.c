
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

void od_backend_close(od_server_t *server)
{
	assert(server->route == NULL);
	assert(server->io.io == NULL);
	assert(server->tls == NULL);
	server->is_transaction = 0;
	server->idle_time = 0;
	kiwi_key_init(&server->key);
	kiwi_key_init(&server->key_client);
	od_server_free(server);
}

static inline int od_backend_terminate(od_server_t *server)
{
	machine_msg_t *msg;
	msg = kiwi_fe_write_terminate(NULL);
	if (msg == NULL)
		return -1;
	return od_write(&server->io, msg);
}

void od_backend_close_connection(od_server_t *server)
{
	assert(server != NULL);
	/* failed to connect to endpoint, so notring to do */
	if (server->io.io == NULL) {
		return;
	}
	if (machine_connected(server->io.io))
		od_backend_terminate(server);

	od_io_close(&server->io);

	if (server->error_connect) {
		machine_msg_free(server->error_connect);
		server->error_connect = NULL;
	}

	if (server->tls) {
		machine_tls_free(server->tls);
		server->tls = NULL;
	}
}

void od_backend_error(od_server_t *server, char *context, char *data,
		      uint32_t size)
{
	od_instance_t *instance = server->global->instance;
	kiwi_fe_error_t error;

	int rc;
	rc = kiwi_fe_read_error(data, size, &error);
	if (rc == -1) {
		od_error(&instance->logger, context, server->client, server,
			 "failed to parse error message from server");
		return;
	}

	od_error(&instance->logger, context, server->client, server, "%s %s %s",
		 error.severity, error.code, error.message);

	if (error.detail) {
		od_error(&instance->logger, context, server->client, server,
			 "DETAIL: %s", error.detail);
	}

	if (error.hint) {
		od_error(&instance->logger, context, server->client, server,
			 "HINT: %s", error.hint);
	}
}

int od_backend_ready(od_server_t *server, char *data, uint32_t size)
{
	int status;
	int rc;
	rc = kiwi_fe_read_ready(data, size, &status);
	if (rc == -1)
		return -1;
	if (status == 'I') {
		/* no active transaction */
		server->is_transaction = 0;
	} else if (status == 'T' || status == 'E') {
		/* in active transaction or in interrupted
		 * transaction block */
		server->is_transaction = 1;
	}

	/* update server sync reply state */

	od_server_sync_reply(server);
	return 0;
}

static inline int od_backend_startup(od_server_t *server,
				     kiwi_params_t *route_params,
				     od_client_t *client)
{
	od_instance_t *instance = server->global->instance;
	od_route_t *route = server->route;

#define DEFAULT_ARGV_SIZE 6

	kiwi_fe_arg_t argv[DEFAULT_ARGV_SIZE +
			   2 * route->rule->backend_startup_vars_sz];

	kiwi_fe_arg_t default_argv[] = {
		{ "user", 5 },
		{ route->id.user, route->id.user_len },
		{ "database", 9 },
		{ route->id.database, route->id.database_len },
		{ "replication", 12 },
		{ NULL, 0 }
	};

	od_debug(&instance->logger, "startup", NULL, server,
		 "startup server connection with user %s & database %s",
		 route->id.user, route->id.database);

	for (size_t i = 0; i < route->rule->backend_startup_vars_sz; i++) {
		argv[i << 1].name = route->rule->backend_startup_vars[i].name;
		argv[i << 1].len =
			route->rule->backend_startup_vars[i].name_len + 1;
		argv[i << 1 | 1].name =
			route->rule->backend_startup_vars[i].value;
		argv[i << 1 | 1].len =
			route->rule->backend_startup_vars[i].value_len + 1;
	}

	int argc = route->rule->backend_startup_vars_sz * 2;

	for (size_t i = 0; i < DEFAULT_ARGV_SIZE; ++i) {
		argv[argc + i] = default_argv[i];
	}

	argc += 4;

	if (route->id.physical_rep) {
		argv[argc + 1].name = "on";
		argv[argc + 1].len = 3;
		argc += 2;
	} else if (route->id.logical_rep) {
		argv[argc + 1].name = "database";
		argv[argc + 1].len = 9;
		argc += 2;
	}

	machine_msg_t *msg;
	msg = kiwi_fe_write_startup_message(NULL, argc, argv);
	if (msg == NULL)
		return -1;
	int rc;
	rc = od_write(&server->io, msg);
	if (rc == -1) {
		od_error(&instance->logger, "startup", NULL, server,
			 "write error: %s", od_io_error(&server->io));
		return -1;
	}

	/* update request count and sync state */
	od_server_sync_request(server, 1);
	assert(server->client);

	for (;;) {
		msg = od_read(&server->io, UINT32_MAX);
		if (msg == NULL) {
			od_error(&instance->logger, "startup", client, server,
				 "read error: %s", od_io_error(&server->io));
			return -1;
		}

		kiwi_be_type_t type = *(char *)machine_msg_data(msg);
		od_debug(&instance->logger, "startup", client, server,
			 "received packet type: %s",
			 kiwi_be_type_to_string(type));

		switch (type) {
		case KIWI_BE_READY_FOR_QUERY:
			od_backend_ready(server, machine_msg_data(msg),
					 machine_msg_size(msg));
			machine_msg_free(msg);
			return 0;
		case KIWI_BE_AUTHENTICATION:
			rc = od_auth_backend(server, msg, client);
			machine_msg_free(msg);
			if (rc == -1)
				return -1;
			break;
		case KIWI_BE_BACKEND_KEY_DATA:
			rc = kiwi_fe_read_key(machine_msg_data(msg),
					      machine_msg_size(msg),
					      &server->key);
			machine_msg_free(msg);
			if (rc == -1) {
				od_error(
					&instance->logger, "startup", client,
					server,
					"failed to parse BackendKeyData message");
				return -1;
			}
			break;
		case KIWI_BE_PARAMETER_STATUS: {
			char *name;
			uint32_t name_len;
			char *value;
			uint32_t value_len;
			rc = kiwi_fe_read_parameter(machine_msg_data(msg),
						    machine_msg_size(msg),
						    &name, &name_len, &value,
						    &value_len);
			if (rc == -1) {
				machine_msg_free(msg);
				od_error(
					&instance->logger, "startup", client,
					server,
					"failed to parse ParameterStatus message");
				return -1;
			}

			/* set server parameters */
			kiwi_vars_update(&server->vars, name, name_len, value,
					 value_len);

			if (route_params) {
				// skip volatile params
				// we skip in_hot_standby here because it may change
				// during connection lifetime, if server was
				// promoted
				if (name_len != sizeof("in_hot_standby") ||
				    strncmp(name, "in_hot_standby", name_len)) {
					kiwi_param_t *param;
					param = kiwi_param_allocate(name,
								    name_len,
								    value,
								    value_len);
					if (param)
						kiwi_params_add(route_params,
								param);
				}
			}

			machine_msg_free(msg);
			break;
		}
		case KIWI_BE_NOTICE_RESPONSE:
			machine_msg_free(msg);
			break;
		case KIWI_BE_ERROR_RESPONSE:
			od_backend_error(server, "startup",
					 machine_msg_data(msg),
					 machine_msg_size(msg));
			server->error_connect = msg;
			return -1;
		default:
			machine_msg_free(msg);
			od_debug(&instance->logger, "startup", client, server,
				 "unexpected message: %s",
				 kiwi_be_type_to_string(type));
			return -1;
		}
	}
	od_unreachable();
	return 0;
}

static inline int od_backend_connect_to(od_server_t *server, char *context,
					char *host, int port,
					od_tls_opts_t *tlsopts)
{
	od_instance_t *instance = server->global->instance;
	assert(server->io.io == NULL);

	/* create io handle */
	machine_io_t *io;
	io = machine_io_create();
	if (io == NULL)
		return -1;

	/* set network options */
	machine_set_nodelay(io, instance->config.nodelay);
	if (instance->config.keepalive > 0) {
		machine_set_keepalive(io, 1, instance->config.keepalive,
				      instance->config.keepalive_keep_interval,
				      instance->config.keepalive_probes,
				      instance->config.keepalive_usr_timeout);
	}

	int rc;
	rc = od_io_prepare(&server->io, io, instance->config.readahead);
	if (rc == -1) {
		od_error(&instance->logger, context, NULL, server,
			 "failed to set server io");
		machine_close(io);
		machine_io_free(io);
		return -1;
	}

	/* set tls options */
	if (tlsopts->tls_mode != OD_CONFIG_TLS_DISABLE) {
		server->tls = od_tls_backend(tlsopts);
		if (server->tls == NULL)
			return -1;
	}

	uint64_t time_connect_start = 0;
	if (instance->config.log_session)
		time_connect_start = machine_time_us();

	struct sockaddr_un saddr_un;
	struct sockaddr_in saddr_v4;
	struct sockaddr_in6 saddr_v6;
	struct sockaddr *saddr;
	struct addrinfo *ai = NULL;

	/* resolve server address */
	if (host) {
		/* assume IPv6 or IPv4 is specified */
		int rc_resolve = -1;
		if (strchr(host, ':')) {
			/* v6 */
			memset(&saddr_v6, 0, sizeof(saddr_v6));
			saddr_v6.sin6_family = AF_INET6;
			saddr_v6.sin6_port = htons(port);
			rc_resolve =
				inet_pton(AF_INET6, host, &saddr_v6.sin6_addr);
			saddr = (struct sockaddr *)&saddr_v6;
		} else {
			/* v4 or hostname */
			memset(&saddr_v4, 0, sizeof(saddr_v4));
			saddr_v4.sin_family = AF_INET;
			saddr_v4.sin_port = htons(port);
			rc_resolve =
				inet_pton(AF_INET, host, &saddr_v4.sin_addr);
			saddr = (struct sockaddr *)&saddr_v4;
		}

		/* schedule getaddrinfo() execution */
		if (rc_resolve != 1) {
			char rport[16];
			od_snprintf(rport, sizeof(rport), "%d", port);

			rc = machine_getaddrinfo(host, rport, NULL, &ai, 0);
			if (rc != 0) {
				od_error(&instance->logger, context, NULL,
					 server, "failed to resolve %s:%d",
					 host, port);
				return NOT_OK_RESPONSE;
			}
			assert(ai != NULL);
			saddr = ai->ai_addr;
		}
		/* connected */

	} else {
		/* set unix socket path */
		memset(&saddr_un, 0, sizeof(saddr_un));
		saddr_un.sun_family = AF_UNIX;
		saddr = (struct sockaddr *)&saddr_un;
		od_snprintf(saddr_un.sun_path, sizeof(saddr_un.sun_path),
			    "%s/.s.PGSQL.%d", instance->config.unix_socket_dir,
			    port);
	}

	uint64_t time_resolve = 0;
	if (instance->config.log_session) {
		time_resolve = machine_time_us() - time_connect_start;
	}

	/* connect to server */
	rc = machine_connect(
		server->io.io, saddr,
		(uint32_t)instance->config.backend_connect_timeout_ms);
	if (ai) {
		freeaddrinfo(ai);
	}

	if (rc == NOT_OK_RESPONSE) {
		if (host) {
			od_error(&instance->logger, context, server->client,
				 server, "failed to connect to %s:%d", host,
				 port);
		} else {
			od_error(&instance->logger, context, server->client,
				 server, "failed to connect to %s",
				 saddr_un.sun_path);
		}
		return NOT_OK_RESPONSE;
	}

	/* do tls handshake */
	if (tlsopts->tls_mode != OD_CONFIG_TLS_DISABLE) {
		rc = od_tls_backend_connect(server, &instance->logger, tlsopts);
		if (rc == NOT_OK_RESPONSE) {
			return NOT_OK_RESPONSE;
		}
	}

	uint64_t time_connect = 0;
	if (instance->config.log_session) {
		time_connect = machine_time_us() - time_connect_start;
	}

	/* log server connection */
	if (instance->config.log_session) {
		if (host) {
			od_log(&instance->logger, context, server->client,
			       server,
			       "new server connection %s:%d (connect time: %d usec, "
			       "resolve time: %d usec)",
			       host, port, (int)time_connect,
			       (int)time_resolve);
		} else {
			od_log(&instance->logger, context, server->client,
			       server,
			       "new server connection %s (connect time: %d usec, resolve "
			       "time: %d usec)",
			       saddr_un.sun_path, (int)time_connect,
			       (int)time_resolve);
		}
	}

	return 0;
}

static inline int od_storage_parse_rw_check_response(machine_msg_t *msg,
						     bool *is_rw)
{
	char *pos = (char *)machine_msg_data(msg) + 1;
	uint32_t pos_size = machine_msg_size(msg) - 1;

	/* size */
	uint32_t size;
	int rc;
	rc = kiwi_read32(&size, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1))
		goto error;
	/* count */
	uint16_t count;
	rc = kiwi_read16(&count, &pos, &pos_size);

	if (kiwi_unlikely(rc == -1))
		goto error;

	if (count != 1)
		goto error;

	/* (not used) */
	uint32_t resp_len;
	rc = kiwi_read32(&resp_len, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1)) {
		goto error;
	}

	/* we expect exactly one row */
	if (resp_len != 1) {
		return NOT_OK_RESPONSE;
	}
	/* pg is in recovery false means db is open for write */
	if (pos[0] == 'f') {
		*is_rw = true;
		return OK_RESPONSE;
	} else if (pos[0] == 't') {
		*is_rw = false;
		return OK_RESPONSE;
	}
	/* fallthrough to error */
error:
	return NOT_OK_RESPONSE;
}

static inline od_retcode_t od_backend_update_endpoint_status(
	od_instance_t *instance, od_client_t *client, od_server_t *server,
	char *context, od_storage_endpoint_t *endpoint, char *host, int port)
{
	od_storage_endpoint_status_t status;

	machine_msg_t *msg;
	msg = od_query_do(server, context, "SELECT pg_is_in_recovery()", NULL);
	if (msg == NULL) {
		od_error(&instance->logger, context, client, server,
			 "can't execute pg_is_in_recovery");
		return NOT_OK_RESPONSE;
	}

	if (od_storage_parse_rw_check_response(msg, &status.is_read_write) !=
	    OK_RESPONSE) {
		od_error(&instance->logger, context, client, server,
			 "can't parse pg_is_in_recovery result");
		machine_msg_free(msg);
		return NOT_OK_RESPONSE;
	}
	machine_msg_free(msg);

	status.last_update_time_ms = machine_time_ms();

	od_storage_endpoint_status_set(&endpoint->status, &status);

	od_log(&instance->logger, context, client, server,
	       "read-write status of %s:%d is updated to %s", host, port,
	       status.is_read_write ? "true" : "false");

	return OK_RESPONSE;
}

static inline od_retcode_t od_backend_attempt_connect_with_tsa(
	od_rule_storage_t *storage, od_server_t *server, char *context,
	kiwi_params_t *route_params, char *host, int port, od_tls_opts_t *opts,
	od_target_session_attrs_t attrs, od_storage_endpoint_t *endpoint,
	od_client_t *client)
{
	od_global_t *global = server->global;
	od_instance_t *instance = global->instance;

	od_retcode_t rc;

	/*
	 * there was a previous faile connection attempt
	 * like not matched tsa
	 * 
	 * we must kept connection not closed after last attempt
	 * because some of its state (like error_connect) can be used even
	 * if connection failed
	*/
	if (server->io.io != NULL) {
		od_backend_close_connection(server);
	}

	rc = od_backend_connect_to(server, context, host, port, opts);
	if (rc == NOT_OK_RESPONSE) {
		return rc;
	}

	/* send startup and do initial configuration */
	rc = od_backend_startup(server, route_params, client);
	if (rc == NOT_OK_RESPONSE) {
		return rc;
	}

	if (attrs == OD_TARGET_SESSION_ATTRS_ANY) {
		return OK_RESPONSE;
	}

	if (od_storage_endpoint_status_is_outdated(
		    &endpoint->status,
		    storage->endpoints_status_poll_interval_ms)) {
		if (od_backend_update_endpoint_status(instance, client, server,
						      context, endpoint, host,
						      port) != OK_RESPONSE) {
			return NOT_OK_RESPONSE;
		}
	}

	od_storage_endpoint_status_t status;
	od_storage_endpoint_status_get(&endpoint->status, &status);

	switch (attrs) {
	case OD_TARGET_SESSION_ATTRS_RW:
		rc = status.is_read_write ? OK_RESPONSE : NOT_OK_RESPONSE;
		break;
	case OD_TARGET_SESSION_ATTRS_RO:
		/* this is primary, but we are forsed to find ro backend */
		rc = status.is_read_write ? NOT_OK_RESPONSE : OK_RESPONSE;
		break;
	default:
		abort();
	}

	return rc;
}

static inline bool
od_backend_endpoint_should_be_skipped(od_rule_storage_t *storage,
				      od_storage_endpoint_t *endpoint,
				      od_target_session_attrs_t target_attrs)
{
	if (target_attrs == OD_TARGET_SESSION_ATTRS_ANY) {
		return false;
	}

	if (od_storage_endpoint_status_is_outdated(
		    &endpoint->status,
		    storage->endpoints_status_poll_interval_ms)) {
		return false;
	}

	od_storage_endpoint_status_t status;
	od_storage_endpoint_status_get(&endpoint->status, &status);

	/* opposite to connection */
	switch (target_attrs) {
	case OD_TARGET_SESSION_ATTRS_RW:
		return !status.is_read_write;
	case OD_TARGET_SESSION_ATTRS_RO:
		/* this is primary, but we are forced to find ro backend */
		return status.is_read_write;
	default:
		abort();
	}
}

static inline uint32_t
od_backend_storage_next_endpoint(od_rule_storage_t *storage)
{
	for (;;) {
		uint32_t v = od_atomic_u32_of(&storage->rr_counter);
		uint32_t next = v + 1 == storage->endpoints_count ? 0 : v + 1;

		if (od_atomic_u32_cas(&storage->rr_counter, v, next) == v) {
			return v;
		}
	}
}

static inline int od_backend_connect_on_matched_endpoint(
	od_rule_storage_t *storage, od_target_session_attrs_t tsa,
	od_server_t *server, char *context, kiwi_params_t *route_params,
	od_client_t *client)
{
	od_instance_t *instance = server->global->instance;

	/* For UNIX socket */
	if (storage->endpoints_count == 0) {
		int rc = od_backend_attempt_connect_with_tsa(
			storage, server, context, route_params, NULL,
			storage->port, storage->tls_opts, tsa,
			NULL /* endpoint */, client);

		if (rc == OK_RESPONSE) {
			server->endpoint_selector = 0;
		}

		return rc;
	}

	for (size_t i = 0; i < storage->endpoints_count; ++i) {
		size_t idx = od_backend_storage_next_endpoint(storage);
		od_storage_endpoint_t *endpoint = &storage->endpoints[idx];

		if (od_backend_endpoint_should_be_skipped(storage, endpoint,
							  tsa)) {
			continue;
		}

		if (od_backend_attempt_connect_with_tsa(
			    storage, server, context, route_params,
			    endpoint->host, endpoint->port, storage->tls_opts,
			    tsa, endpoint, client) == NOT_OK_RESPONSE) {
			continue;
		}

		/* target host found! */
		od_debug(&instance->logger, context, NULL, server,
			 "%s found on %s:%d",
			 od_target_session_attrs_to_pg_mode_str(tsa),
			 endpoint->host, endpoint->port);

		server->endpoint_selector = idx;
		return OK_RESPONSE;
	}

	od_debug(&instance->logger, context, NULL, server,
		 "failed to find %s within %s",
		 od_target_session_attrs_to_pg_mode_str(tsa), storage->host);

	return NOT_OK_RESPONSE;
}

int od_backend_connect(od_server_t *server, char *context,
		       kiwi_params_t *route_params, od_client_t *client,
		       od_target_session_attrs_t default_tsa)
{
	od_route_t *route = server->route;
	assert(route != NULL);

	od_rule_storage_t *storage;
	storage = route->rule->storage;
	od_target_session_attrs_t effective_tsa = OD_TARGET_SESSION_ATTRS_UNDEF;

	if (route->rule->target_session_attrs !=
	    OD_TARGET_SESSION_ATTRS_UNDEF) {
		effective_tsa = route->rule->target_session_attrs;
	}

	/* if listen config specifies tsa, it is considered as more powerfull default */
	if (default_tsa != OD_TARGET_SESSION_ATTRS_UNDEF) {
		effective_tsa = default_tsa;
	}

	kiwi_var_t *hint_var = kiwi_vars_get(
		&client->vars, KIWI_VAR_ODYSSEY_TARGET_SESSION_ATTRS);

	/* XXX: TODO refactor kiwi_vars_get to avoid strcmp */
	if (hint_var != NULL) {
		if (strncmp(hint_var->value, "read-only",
			    hint_var->value_len) == 0) {
			effective_tsa = OD_TARGET_SESSION_ATTRS_RO;
		}
		if (strncmp(hint_var->value, "read-write",
			    hint_var->value_len) == 0) {
			effective_tsa = OD_TARGET_SESSION_ATTRS_RW;
		}

		if (strncmp(hint_var->value, "any", hint_var->value_len) == 0) {
			effective_tsa = OD_TARGET_SESSION_ATTRS_ANY;
		}
	}

	/* 'read-write' and 'read-only' is passed as is, 'any' or unknown == any */

	if (effective_tsa == OD_TARGET_SESSION_ATTRS_UNDEF) {
		effective_tsa = OD_TARGET_SESSION_ATTRS_ANY;
	}

	return od_backend_connect_on_matched_endpoint(
		storage, effective_tsa, server, context, route_params, client);
}

int od_backend_connect_service(od_server_t *server, char *context,
			       kiwi_params_t *route_params, od_client_t *client)
{
	return od_backend_connect(server, context, route_params, client,
				  OD_TARGET_SESSION_ATTRS_ANY);
}

int od_backend_connect_cancel(od_server_t *server, od_rule_storage_t *storage,
			      kiwi_key_t *key)
{
	od_instance_t *instance = server->global->instance;
	/* connect to server */
	int rc;
	char *host = NULL; /* For UNIX socket */
	int port = storage->port;
	if (storage->endpoints_count) {
		host = storage->endpoints[server->endpoint_selector].host;
		if (storage->endpoints[server->endpoint_selector].port)
			port = storage->endpoints[server->endpoint_selector]
				       .port;
	}
	rc = od_backend_connect_to(server, "cancel", host, port,
				   storage->tls_opts);
	if (rc == NOT_OK_RESPONSE) {
		return NOT_OK_RESPONSE;
	}

	/* send cancel request */
	machine_msg_t *msg;
	msg = kiwi_fe_write_cancel(NULL, key->key_pid, key->key);
	if (msg == NULL)
		return -1;

	rc = od_write(&server->io, msg);
	if (rc == -1) {
		od_error(&instance->logger, "cancel", NULL, NULL,
			 "write error: %s", od_io_error(&server->io));
		return -1;
	}

	return 0;
}

int od_backend_update_parameter(od_server_t *server, char *context, char *data,
				uint32_t size, int server_only)
{
	od_instance_t *instance = server->global->instance;
	od_client_t *client = server->client;

	char *name;
	uint32_t name_len;
	char *value;
	uint32_t value_len;

	int rc;
	rc = kiwi_fe_read_parameter(data, size, &name, &name_len, &value,
				    &value_len);
	if (rc == -1) {
		od_error(&instance->logger, context, NULL, server,
			 "failed to parse ParameterStatus message");
		return -1;
	}

	/* update server only or client and server parameter */
	od_debug(&instance->logger, context, client, server, "%.*s = %.*s",
		 name_len, name, value_len, value);

	if (server_only) {
		kiwi_vars_update(&server->vars, name, name_len, value,
				 value_len);
	} else {
		kiwi_vars_update_both(&client->vars, &server->vars, name,
				      name_len, value, value_len);
	}

	return 0;
}

int od_backend_ready_wait(od_server_t *server, char *context, uint32_t time_ms,
			  uint32_t ignore_errors)
{
	od_instance_t *instance = server->global->instance;
	int query_rc;
	query_rc = 0;

	for (; !od_server_synchronized(server);) {
		machine_msg_t *msg;
		msg = od_read(&server->io, time_ms);
		if (msg == NULL) {
			if (!machine_timedout()) {
				od_error(&instance->logger, context,
					 server->client, server,
					 "read error: %s",
					 od_io_error(&server->io));
			}
			return -1;
		}
		kiwi_be_type_t type = *(char *)machine_msg_data(msg);
		od_debug(&instance->logger, context, server->client, server,
			 "%s", kiwi_be_type_to_string(type));

		if (type == KIWI_BE_PARAMETER_STATUS) {
			/* update server parameter */
			int rc;
			rc = od_backend_update_parameter(server, context,
							 machine_msg_data(msg),
							 machine_msg_size(msg),
							 1);
			machine_msg_free(msg);
			if (rc == -1) {
				return -1;
			}
		} else if (type == KIWI_BE_ERROR_RESPONSE) {
			od_backend_error(server, context, machine_msg_data(msg),
					 machine_msg_size(msg));
			machine_msg_free(msg);
			if (!ignore_errors) {
				query_rc = -1;
			}
		} else if (type == KIWI_BE_READY_FOR_QUERY) {
			od_backend_ready(server, machine_msg_data(msg),
					 machine_msg_size(msg));
			machine_msg_free(msg);
		} else {
			machine_msg_free(msg);
		}
	}

	return query_rc;
}

od_retcode_t od_backend_query_send(od_server_t *server, char *context,
				   char *query, char *param, int len)
{
	od_instance_t *instance = server->global->instance;

	machine_msg_t *msg;
	if (param) {
		msg = kiwi_fe_write_prep_stmt(NULL, query, param);
	} else {
		msg = kiwi_fe_write_query(NULL, query, len);
	}

	if (msg == NULL) {
		return NOT_OK_RESPONSE;
	}

	int rc;
	rc = od_write(&server->io, msg);
	if (rc == -1) {
		od_error(&instance->logger, context, server->client, server,
			 "write error: %s", od_io_error(&server->io));
		return NOT_OK_RESPONSE;
	}

	/* update server sync state */
	od_server_sync_request(server, 1);
	assert(server->client);
	return OK_RESPONSE;
}

od_retcode_t od_backend_query(od_server_t *server, char *context, char *query,
			      char *param, int len, uint32_t timeout,
			      uint32_t ignore_errors)
{
	if (od_backend_query_send(server, context, query, param, len) ==
	    NOT_OK_RESPONSE) {
		return NOT_OK_RESPONSE;
	}

	od_retcode_t rc =
		od_backend_ready_wait(server, context, timeout, ignore_errors);
	return rc;
}
