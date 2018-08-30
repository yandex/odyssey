
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

static inline int
od_auth_query_do(od_server_t *server, char *query, int len,
                 kiwi_password_t *result)
{
	od_instance_t *instance = server->global->instance;

	od_debug(&instance->logger, "auth_query", server->client, server,
	         "%s", query);

	machine_msg_t *msg;
	msg = kiwi_fe_write_query(query, len);
	if (msg == NULL)
		return -1;
	int rc;
	rc = machine_write(server->io, msg);
	if (rc == -1) {
		od_error(&instance->logger, "auth_query", server->client, server,
		         "write error: %s",
		         machine_error(server->io));
		return -1;
	}
	rc = machine_flush(server->io, UINT32_MAX);
	if (rc == -1) {
		od_error(&instance->logger, "auth_query", server->client, server,
		         "write error: %s",
		         machine_error(server->io));
		return -1;
	}

	/* update server sync state */
	od_server_sync_request(server, 1);

	/* wait for response */
	int has_result = 0;
	while (1)
	{
		msg = od_read(server->io, UINT32_MAX);
		if (msg == NULL) {
			if (! machine_timedout()) {
				od_error(&instance->logger, "auth_query", server->client, server,
				         "read error: %s",
				         machine_error(server->io));
			}
			return -1;
		}
		kiwi_be_type_t type;
		type = *(char*)machine_msg_get_data(msg);

		od_debug(&instance->logger, "auth_query", server->client, server, "%s",
		         kiwi_be_type_to_string(type));

		switch (type) {
		case KIWI_BE_ERROR_RESPONSE:
			od_backend_error(server, "auth_query", msg);
			goto error;
		case KIWI_BE_ROW_DESCRIPTION:
			break;
		case KIWI_BE_DATA_ROW:
		{
			if (has_result)
				goto error;
			char *pos = (char*)machine_msg_get_data(msg) + 1;
			uint32_t pos_size = machine_msg_get_size(msg)- 1;

			/* size */
			uint32_t size;
			rc = kiwi_read32(&size, &pos, &pos_size);
			if (kiwi_unlikely(rc == -1))
				goto error;
			/* count */
			uint16_t count;
			rc = kiwi_read16(&count, &pos, &pos_size);
			if (kiwi_unlikely(rc == -1))
				goto error;
			if (count != 2)
				goto error;

			/* user (not used) */
			uint32_t user_len;
			rc = kiwi_read32(&user_len, &pos, &pos_size);
			if (kiwi_unlikely(rc == -1))
				goto error;
			char *user = pos;
			rc = kiwi_readn(user_len, &pos, &pos_size);
			if (kiwi_unlikely(rc == -1))
				goto error;
			(void)user;
			(void)user_len;

			/* password */
			uint32_t password_len;
			rc = kiwi_read32(&password_len, &pos, &pos_size);
			if (kiwi_unlikely(rc == -1))
				goto error;
			char *password = pos;
			rc = kiwi_readn(password_len, &pos, &pos_size);
			if (kiwi_unlikely(rc == -1))
				goto error;

			result->password = malloc(password_len + 1);
			if (result->password == NULL)
				goto error;
			memcpy(result->password, password, password_len);
			result->password[password_len] = 0;
			result->password_len = password_len + 1;

			has_result = 1;
			break;
		}
		case KIWI_BE_READY_FOR_QUERY:
			od_backend_ready(server, msg);
			machine_msg_free(msg);
			return 0;
		default:
			break;
		}

		machine_msg_free(msg);
	}
	return 0;

error:
	machine_msg_free(msg);
	return -1;
}

__attribute__((hot)) static inline int
od_auth_query_format(od_config_route_t *config, kiwi_param_t *user,
                     char *output, int output_len)
{
	char *dst_pos = output;
	char *dst_end = output + output_len;
	char *format_pos = config->auth_query;
	char *format_end = config->auth_query + strlen(config->auth_query);
	while (format_pos < format_end)
	{
		if (*format_pos == '%') {
			format_pos++;
			if (od_unlikely(format_pos == format_end))
				break;
			if (*format_pos == 'u') {
				int len;
				len = od_snprintf(dst_pos, dst_end - dst_pos, "%s",
				                  kiwi_param_value(user));
				dst_pos += len;
			} else {
				if (od_unlikely((dst_end - dst_pos) < 2))
					break;
				dst_pos[0] = '%';
				dst_pos[1] = *format_pos;
				dst_pos   += 2;
			}
		} else {
			if (od_unlikely((dst_end - dst_pos) < 1))
				break;
			dst_pos[0] = *format_pos;
			dst_pos   += 1;
		}
		format_pos++;
	}
	if (od_unlikely((dst_end - dst_pos) < 1))
		return -1;
	dst_pos[0] = 0;
	dst_pos++;
	return dst_pos - output;
}

int
od_auth_query(od_global_t *global,
              od_config_route_t *config,
              kiwi_param_t *user,
              kiwi_password_t *password)
{
	od_instance_t *instance = global->instance;

	/* create internal auth client */
	od_client_t *auth_client;
	auth_client = od_client_allocate();
	if (auth_client == NULL)
		return -1;
	auth_client->global = global;
	od_id_mgr_generate(&instance->id_mgr, &auth_client->id, "a");

	/* set auth query route db and user */
	kiwi_param_t *query_db;
	query_db = kiwi_param_allocate("database", 9, config->auth_query_db,
	                               strlen(config->auth_query_db) + 1);
	if (query_db == NULL) {
		od_client_free(auth_client);
		return -1;
	}
	kiwi_params_add(&auth_client->startup.params, query_db);
	kiwi_param_t *query_user;
	query_user = kiwi_param_allocate("user", 5, config->auth_query_user,
	                                 strlen(config->auth_query_user) + 1);
	if (query_user == NULL) {
		od_client_free(auth_client);
		return -1;
	}
	kiwi_params_add(&auth_client->startup.params, query_user);

	auth_client->startup.database = query_db;
	auth_client->startup.user = query_user;

	/* route */
	od_router_status_t status;
	status = od_route(auth_client);
	if (status != OD_ROK) {
		od_client_free(auth_client);
		return -1;
	}

	/* attach */
	status = od_router_attach(auth_client);
	if (status != OD_ROK) {
		od_unroute(auth_client);
		od_client_free(auth_client);
		return -1;
	}
	od_server_t *server;
	server = auth_client->server;

	od_debug(&instance->logger, "auth_query", NULL, server,
	         "attached to %s%.*s",
	         server->id.id_prefix, sizeof(server->id.id),
	         server->id.id);

	/* connect to server, if necessary */
	int rc;
	if (server->io == NULL) {
		rc = od_backend_connect(server, "auth_query");
		if (rc == -1) {
			od_router_close_and_unroute(auth_client);
			od_client_free(auth_client);
			return -1;
		}
	}

	/* preformat and execute query */
	char query[512];
	int  query_len;
	query_len = od_auth_query_format(config, user, query, sizeof(query));

	rc = od_auth_query_do(server, query, query_len, password);
	if (rc == -1) {
		od_router_close_and_unroute(auth_client);
		od_client_free(auth_client);
		return -1;
	}

	/* detach and unroute */
	od_router_detach_and_unroute(auth_client);
	od_client_free(auth_client);
	return 0;
}
