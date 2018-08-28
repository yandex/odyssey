
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

void
od_backend_close(od_server_t *server)
{
	assert(server->route == NULL);
	assert(server->io == NULL);
	assert(server->tls == NULL);
	server->is_transaction = 0;
	server->idle_time = 0;
	kiwi_key_init(&server->key);
	kiwi_key_init(&server->key_client);
	od_server_free(server);
}

static inline int
od_backend_terminate(od_server_t *server)
{
	machine_msg_t *msg;
	msg = kiwi_fe_write_terminate();
	if (msg == NULL)
		return -1;
	int rc;
	rc = machine_write(server->io, msg);
	if (rc == -1)
		return -1;
	rc = machine_flush(server->io, UINT32_MAX);
	return rc;
}

void
od_backend_close_connection(od_server_t *server)
{
	if (server->io == NULL)
		return;

	if (machine_connected(server->io))
		od_backend_terminate(server);

	machine_close(server->io);
	machine_io_free(server->io);
	server->io = NULL;

	if (server->tls) {
		machine_tls_free(server->tls);
		server->tls = NULL;
	}
}

void
od_backend_error(od_server_t *server, char *context,
                 machine_msg_t *msg)
{
	od_instance_t *instance = server->global->instance;
	kiwi_fe_error_t error;
	int rc;
	rc = kiwi_fe_read_error(msg, &error);
	if (rc == -1) {
		od_error(&instance->logger, context, server->client, server,
		         "failed to parse error message from server");
		return;
	}
	od_error(&instance->logger, context, server->client, server,
	         "%s %s %s",
	         error.severity,
	         error.code,
	         error.message);
	if (error.detail) {
		od_error(&instance->logger, context, server->client, server,
		         "DETAIL: %s", error.detail);
	}
	if (error.hint) {
		od_error(&instance->logger, context, server->client, server,
		         "HINT: %s", error.hint);
	}
}

int
od_backend_ready(od_server_t *server, machine_msg_t *msg)
{
	int status;
	int rc;
	rc = kiwi_fe_read_ready(msg, &status);
	if (rc == -1)
		return -1;
	if (status == 'I') {
		/* no active transaction */
		server->is_transaction = 0;
	} else
	if (status == 'T' || status == 'E') {
		/* in active transaction or in interrupted
		 * transaction block */
		server->is_transaction = 1;
	}

	/* update server sync reply state */
	od_server_sync_reply(server);
	return 0;
}

static inline int
od_backend_startup(od_server_t *server)
{
	od_instance_t *instance = server->global->instance;
	od_route_t *route = server->route;

	kiwi_fe_arg_t argv[] = {
		{ "user", 5 },     { route->id.user, route->id.user_len },
		{ "database", 9 }, { route->id.database, route->id.database_len }
	};

	machine_msg_t *msg;
	msg = kiwi_fe_write_startup_message(4, argv);
	if (msg == NULL)
		return -1;
	int rc;
	rc = machine_write(server->io, msg);
	if (rc == -1) {
		od_error(&instance->logger, "startup", NULL, server,
		         "write error: %s",
		         machine_error(server->io));
		return -1;
	}
	rc = machine_flush(server->io, UINT32_MAX);
	if (rc == -1) {
		od_error(&instance->logger, "startup", NULL, server,
		         "write error: %s",
		         machine_error(server->io));
		return -1;
	}

	/* update request count and sync state */
	od_server_sync_request(server, 1);

	while (1)
	{
		msg = od_read(server->io, UINT32_MAX);
		if (msg == NULL) {
			od_error(&instance->logger, "startup", NULL, server,
			         "read error: %s",
			         machine_error(server->io));
			return -1;
		}
		kiwi_be_type_t type = *(char*)machine_msg_get_data(msg);
		od_debug(&instance->logger, "startup", NULL, server, "%s",
		         kiwi_be_type_to_string(type));

		switch (type) {
		case KIWI_BE_READY_FOR_QUERY:
			od_backend_ready(server, msg);
			machine_msg_free(msg);
			return 0;
		case KIWI_BE_AUTHENTICATION:
			rc = od_auth_backend(server, msg);
			machine_msg_free(msg);
			if (rc == -1)
				return -1;
			break;
		case KIWI_BE_BACKEND_KEY_DATA:
			rc = kiwi_fe_read_key(msg, &server->key);
			machine_msg_free(msg);
			if (rc == -1) {
				od_error(&instance->logger, "startup", NULL, server,
				         "failed to parse BackendKeyData message");
				return -1;
			}
			break;
		case KIWI_BE_PARAMETER_STATUS:
		{
			char *name;
			uint32_t name_len;
			char *value;
			uint32_t value_len;
			rc = kiwi_fe_read_parameter(msg, &name, &name_len, &value, &value_len);
			if (rc == -1) {
				machine_msg_free(msg);
				od_error(&instance->logger, "startup", NULL, server,
				         "failed to parse ParameterStatus message");
				return -1;
			}
			kiwi_param_t *param;
			param = kiwi_param_allocate(name, name_len, value, value_len);
			machine_msg_free(msg);
			if (param == NULL)
				return -1;
			kiwi_params_add(&server->params, param);
			break;
		}
		case KIWI_BE_NOTICE_RESPONSE:
			machine_msg_free(msg);
			break;
		case KIWI_BE_ERROR_RESPONSE:
			od_backend_error(server, "startup", msg);
			machine_msg_free(msg);
			return -1;
		default:
			machine_msg_free(msg);
			od_debug(&instance->logger, "startup", NULL, server,
			         "unexpected message: %s",
			         kiwi_be_type_to_string(type));
			return -1;
		}
	}
	return 0;
}

static inline int
od_backend_connect_to(od_server_t *server,
                      char *context,
                      od_config_storage_t *server_config)
{
	od_instance_t *instance = server->global->instance;
	assert(server->io == NULL);

	/* create io handle */
	server->io = machine_io_create();
	if (server->io == NULL)
		return -1;

	/* set network options */
	machine_set_nodelay(server->io, instance->config.nodelay);
	if (instance->config.keepalive > 0)
		machine_set_keepalive(server->io, 1, instance->config.keepalive);
	int rc;
	rc = machine_set_readahead(server->io, instance->config.readahead);
	if (rc == -1) {
		od_error(&instance->logger, context, NULL, server,
		         "failed to set readahead");
		return -1;
	}

	/* set tls options */
	if (server_config->tls_mode != OD_TLS_DISABLE) {
		server->tls = od_tls_backend(server_config);
		if (server->tls == NULL)
			return -1;
	}

	struct sockaddr_un saddr_un;
	struct sockaddr *saddr;
	struct addrinfo *ai = NULL;
	if (server_config->host) {
		/* resolve server address */
		char port[16];
		od_snprintf(port, sizeof(port), "%d", server_config->port);
		rc = machine_getaddrinfo(server_config->host, port, NULL, &ai, 0);
		if (rc != 0) {
			od_error(&instance->logger, context, NULL, server,
			         "failed to resolve %s:%d",
			         server_config->host,
			         server_config->port);
			return -1;
		}
		assert(ai != NULL);
		saddr = ai->ai_addr;
	} else {
		/* set unix socket path */
		memset(&saddr_un, 0, sizeof(saddr_un));
		saddr_un.sun_family = AF_UNIX;
		saddr = (struct sockaddr*)&saddr_un;
		od_snprintf(saddr_un.sun_path, sizeof(saddr_un.sun_path),
		            "%s/.s.PGSQL.%d",
		            instance->config.unix_socket_dir,
		            server_config->port);
	}

	/* connect to server */
	rc = machine_connect(server->io, saddr, UINT32_MAX);
	if (ai)
		freeaddrinfo(ai);
	if (rc == -1) {
		if (server_config->host) {
			od_error(&instance->logger, context, server->client, server,
			         "failed to connect to %s:%d", server_config->host,
			         server_config->port);
		} else {
			od_error(&instance->logger, context, server->client, server,
			         "failed to connect to %s", saddr_un.sun_path);
		}
		return -1;
	}

	/* do tls handshake */
	if (server_config->tls_mode != OD_TLS_DISABLE) {
		rc = od_tls_backend_connect(server, &instance->logger, server_config);
		if (rc == -1)
			return -1;
	}

	/* log server connection */
	if (instance->config.log_session) {
		if (server_config->host) {
			od_log(&instance->logger, context, server->client, server,
			       "new server connection %s:%d", server_config->host,
			       server_config->port);
		} else {
			od_log(&instance->logger, context, server->client, server,
			       "new server connection %s", saddr_un.sun_path);
		}
	}

	return 0;
}

int
od_backend_connect(od_server_t *server, char *context)
{
	od_route_t *route = server->route;
	assert(route != NULL);

	od_config_storage_t *server_config;
	server_config = route->config->storage;

	/* connect to server */
	int rc;
	rc = od_backend_connect_to(server, context, server_config);
	if (rc == -1)
		return -1;

	/* send startup and do initial configuration */
	rc = od_backend_startup(server);
	return rc;
}

int
od_backend_connect_cancel(od_server_t *server,
                          od_config_storage_t *server_config,
                          kiwi_key_t *key)
{
	od_instance_t *instance = server->global->instance;
	/* connect to server */
	int rc;
	rc = od_backend_connect_to(server, "cancel", server_config);
	if (rc == -1)
		return -1;
	/* send cancel request */
	machine_msg_t *msg;
	msg = kiwi_fe_write_cancel(key->key_pid, key->key);
	if (msg == NULL)
		return -1;
	rc = machine_write(server->io, msg);
	if (rc == -1) {
		od_error(&instance->logger, "cancel", NULL, NULL,
		         "write error: %s",
		         machine_error(server->io));
		return -1;
	}
	rc = machine_flush(server->io, UINT32_MAX);
	if (rc == -1) {
		od_error(&instance->logger, "cancel", NULL, NULL,
		         "write error: %s",
		         machine_error(server->io));
		return -1;
	}
	return 0;
}

int
od_backend_ready_wait(od_server_t *server, char *context, int count,
                      uint32_t time_ms)
{
	od_instance_t *instance = server->global->instance;

	int ready = 0;
	for (;;)
	{
		machine_msg_t *msg;
		msg = od_read(server->io, time_ms);
		if (msg == NULL) {
			if (! machine_timedout()) {
				od_error(&instance->logger, context, server->client, server,
				         "read error: %s",
				         machine_error(server->io));
			}
			return -1;
		}
		kiwi_be_type_t type = *(char*)machine_msg_get_data(msg);
		od_debug(&instance->logger, context, server->client, server, "%s",
		         kiwi_be_type_to_string(type));

		if (type == KIWI_BE_ERROR_RESPONSE) {
			od_backend_error(server, context, msg);
			machine_msg_free(msg);
			continue;
		}
		if (type == KIWI_BE_PARAMETER_STATUS)
		{
			char *name;
			uint32_t name_len;
			char *value;
			uint32_t value_len;
			int rc;
			rc = kiwi_fe_read_parameter(msg, &name, &name_len, &value, &value_len);
			if (rc == -1) {
				machine_msg_free(msg);
				od_error(&instance->logger, context, server->client, server,
				         "failed to parse ParameterStatus message");
				return -1;
			}
			od_debug(&instance->logger, context, server->client, server,
			         "%.*s = %.*s",
			         name_len, name, value_len, value);

			/* update parameter */
			kiwi_param_t *param;
			param = kiwi_param_allocate(name, name_len, value, value_len);
			machine_msg_free(msg);
			if (param == NULL)
				return -1;
			kiwi_params_replace(&server->params, param);
			continue;
		}
		if (type == KIWI_BE_READY_FOR_QUERY) {
			od_backend_ready(server, msg);
			ready++;
			if (ready == count) {
				machine_msg_free(msg);
				break;
			}
		}
		machine_msg_free(msg);
	}
	return 0;
}

int
od_backend_query(od_server_t *server, char *context, char *query, int len)
{
	od_instance_t *instance = server->global->instance;

	machine_msg_t *msg;
	msg = kiwi_fe_write_query(query, len);
	if (msg == NULL)
		return -1;
	int rc;
	rc = machine_write(server->io, msg);
	if (rc == -1) {
		od_error(&instance->logger, context, server->client, server,
		         "write error: %s",
		         machine_error(server->io));
		return -1;
	}
	rc = machine_flush(server->io, UINT32_MAX);
	if (rc == -1) {
		od_error(&instance->logger, context, server->client, server,
		         "write error: %s",
		         machine_error(server->io));
		return -1;
	}

	/* update server sync state */
	od_server_sync_request(server, 1);

	rc = od_backend_ready_wait(server, context, 1, UINT32_MAX);
	return rc;
}

int
od_backend_deploy(od_server_t *server, char *context,
                  machine_msg_t *msg)
{
	od_instance_t *instance = server->global->instance;
	int rc;
	switch (*(char*)machine_msg_get_data(msg)) {
	case KIWI_BE_PARAMETER_STATUS:
	{
		char *name;
		uint32_t name_len;
		char *value;
		uint32_t value_len;
		rc = kiwi_fe_read_parameter(msg, &name, &name_len, &value, &value_len);
		if (rc == -1) {
			od_error(&instance->logger, context, server->client, server,
			         "failed to parse ParameterStatus message");
			return -1;
		}
		od_debug(&instance->logger, context, server->client, server,
		         "(deploy) %.*s = %.*s",
		         name_len, name, value_len, value);

		/* update parameter */
		kiwi_param_t *param;
		param = kiwi_param_allocate(name, name_len, value, value_len);
		if (param == NULL)
			return -1;
		kiwi_params_replace(&server->params, param);
		break;
	}
	case KIWI_BE_ERROR_RESPONSE:
		od_backend_error(server, context, msg);
		break;
	case KIWI_BE_READY_FOR_QUERY:
		rc = od_backend_ready(server, msg);
		if (rc == -1)
			return -1;
		server->deploy_sync--;
		break;
	}
	return 0;
}

int
od_backend_deploy_wait(od_server_t *server, char *context,
                       uint32_t time_ms)
{
	od_instance_t *instance = server->global->instance;
	while (server->deploy_sync > 0)
	{
		machine_msg_t *msg;
		msg = od_read(server->io, time_ms);
		if (msg == NULL) {
			if (! machine_timedout()) {
				od_error(&instance->logger, context, server->client, server,
				         "read error: %s",
				         machine_error(server->io));
			}
			return -1;
		}
		int rc;
		rc = od_backend_deploy(server, context, msg);
		machine_msg_free(msg);
		if (rc == -1)
			return -1;
	}
	return 0;
}
