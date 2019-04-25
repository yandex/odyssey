
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

#include <odyssey.h>

static inline int
od_auth_query_do(od_server_t *server, char *query, int len,
                 kiwi_password_t *result)
{
	od_instance_t *instance = server->global->instance;

	od_debug(&instance->logger, "auth_query", server->client, server,
	         "%s", query);

	machine_msg_t *msg;
	msg = kiwi_fe_write_query(NULL, query, len);
	if (msg == NULL)
		return -1;
	int rc;
	rc = od_write(&server->io, msg);
	if (rc == -1) {
		od_error(&instance->logger, "auth_query", server->client, server,
		         "write error: %s",
		         od_io_error(&server->io));
		return -1;
	}

	/* update server sync state */
	od_server_sync_request(server, 1);

	/* wait for response */
	int has_result = 0;
	while (1)
	{
		msg = od_read(&server->io, UINT32_MAX);
		if (msg == NULL) {
			if (! machine_timedout()) {
				od_error(&instance->logger, "auth_query", server->client, server,
				         "read error: %s",
				         od_io_error(&server->io));
			}
			return -1;
		}
		kiwi_be_type_t type;
		type = *(char*)machine_msg_data(msg);

		od_debug(&instance->logger, "auth_query", server->client, server, "%s",
		         kiwi_be_type_to_string(type));

		switch (type) {
		case KIWI_BE_ERROR_RESPONSE:
			od_backend_error(server, "auth_query", machine_msg_data(msg),
			                 machine_msg_size(msg));
			goto error;
		case KIWI_BE_ROW_DESCRIPTION:
			break;
		case KIWI_BE_DATA_ROW:
		{
			if (has_result)
				goto error;
			char *pos = (char*)machine_msg_data(msg) + 1;
			uint32_t pos_size = machine_msg_size(msg) - 1;

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
			od_backend_ready(server, machine_msg_data(msg), machine_msg_size(msg));

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
od_auth_query_format(od_rule_t *rule, kiwi_var_t *user,
                     char *output, int output_len)
{
	char *dst_pos = output;
	char *dst_end = output + output_len;
	char *format_pos = rule->auth_query;
	char *format_end = rule->auth_query + strlen(rule->auth_query);
	while (format_pos < format_end)
	{
		if (*format_pos == '%') {
			format_pos++;
			if (od_unlikely(format_pos == format_end))
				break;
			if (*format_pos == 'u') {
				int len;
				len = od_snprintf(dst_pos, dst_end - dst_pos, "%s",
				                  user->value);
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
od_auth_query(od_global_t *global, od_rule_t *rule,
              kiwi_var_t *user,
              kiwi_password_t *password)
{
	od_instance_t *instance = global->instance;
	od_router_t *router = global->router;

	/* create internal auth client */
	od_client_t *auth_client;
	auth_client = od_client_allocate(instance->top_mcxt);
	if (auth_client == NULL)
		return -1;
	auth_client->global = global;
	od_id_generate(&auth_client->id, "a");

	/* set auth query route user and database */
	kiwi_var_set(&auth_client->startup.user, KIWI_VAR_UNDEF,
	             rule->auth_query_user,
	             strlen(rule->auth_query_user) + 1);

	kiwi_var_set(&auth_client->startup.database, KIWI_VAR_UNDEF,
	             rule->auth_query_db,
	             strlen(rule->auth_query_db) + 1);

	/* route */
	od_router_status_t status;
	status = od_router_route(router, instance->config, auth_client);
	if (status != OD_ROUTER_OK) {
		od_client_free(auth_client);
		return -1;
	}

	/* attach */
	status = od_router_attach(router, instance->config, auth_client);
	if (status != OD_ROUTER_OK) {
		od_router_unroute(router, auth_client);
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
	if (server->io.io == NULL) {
		rc = od_backend_connect(server, "auth_query", NULL);
		if (rc == -1) {
			od_router_close(router, auth_client);
			od_router_unroute(router, auth_client);
			od_client_free(auth_client);
			return -1;
		}
	}

	/* preformat and execute query */
	char query[512];
	int  query_len;
	query_len = od_auth_query_format(rule, user, query, sizeof(query));

	rc = od_auth_query_do(server, query, query_len, password);
	if (rc == -1) {
		od_router_close(router, auth_client);
		od_router_unroute(router, auth_client);
		od_client_free(auth_client);
		return -1;
	}

	/* detach and unroute */
	od_router_detach(router, instance->config, auth_client);
	od_router_unroute(router, auth_client);
	od_client_free(auth_client);
	return 0;
}
