
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

int 
od_query_do(od_server_t *server, char *query,
				   kiwi_var_t *user, kiwi_password_t *result)
{
	od_instance_t *instance = server->global->instance;

	od_debug(&instance->logger, "query", server->client, server, "%s",
		 query);

	machine_msg_t *msg;
	msg = kiwi_fe_write_auth_query(NULL, query, user->value);
	if (msg == NULL)
		return -1;
	int rc;
	rc = od_write(&server->io, msg);
	if (rc == -1) {
		od_error(&instance->logger, "query", server->client,
			 server, "write error: %s", od_io_error(&server->io));
		return -1;
	}

	/* update server sync state */
	od_server_sync_request(server, 1);

	/* wait for response */
	int has_result = 0;
	while (1) {
		msg = od_read(&server->io, UINT32_MAX);
		if (msg == NULL) {
			if (!machine_timedout()) {
				od_error(&instance->logger, "query",
					 server->client, server,
					 "read error: %s",
					 od_io_error(&server->io));
			}
			return -1;
		}
		kiwi_be_type_t type;
		type = *(char *)machine_msg_data(msg);

		od_debug(&instance->logger, "query", server->client,
			 server, "%s", kiwi_be_type_to_string(type));

		switch (type) {
		case KIWI_BE_ERROR_RESPONSE:
			od_backend_error(server, "query",
					 machine_msg_data(msg),
					 machine_msg_size(msg));
			goto error;
		case KIWI_BE_ROW_DESCRIPTION:
			break;
		case KIWI_BE_DATA_ROW: {
			if (has_result)
				goto error;
			char *pos = (char *)machine_msg_data(msg) + 1;
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
			if (kiwi_unlikely(rc == -1)) {
				goto error;
			}
			char *user = pos;
			rc = kiwi_readn(user_len, &pos, &pos_size);
			if (kiwi_unlikely(rc == -1))
				goto error;
			(void)user;
			(void)user_len;

			/* password */
			uint32_t password_len;
			rc = kiwi_read32(&password_len, &pos, &pos_size);

			if (password_len == -1) {
				result->password = NULL;
				result->password_len = password_len + 1;

				od_debug(
					&instance->logger, "query",
					server->client, server,
					"query returned empty password for user : %s",
					user, result->password);
				has_result = 1;
				break;
			}
			if (password_len >
			    ODYSSEY_AUTH_QUERY_MAX_PASSSWORD_LEN) {
				goto error;
			}
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
			od_backend_ready(server, machine_msg_data(msg),
					 machine_msg_size(msg));

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

__attribute__((hot)) int
od_query_format(char *format_pos, char *format_end, kiwi_var_t *user, char *peer,
		     char *output, int output_len)
{
	char *dst_pos = output;
	char *dst_end = output + output_len;
	while (format_pos < format_end) {
		if (*format_pos == '%') {
			format_pos++;
			if (od_unlikely(format_pos == format_end))
				break;
			int len;
			switch (*format_pos) {
			case 'u':
				len = od_snprintf(dst_pos, dst_end - dst_pos,
						  "%s", user->value);
				dst_pos += len;
				break;
			case 'h':
				len = od_snprintf(dst_pos, dst_end - dst_pos,
						  "%s", peer);
				dst_pos += len;
				break;
			default:
				if (od_unlikely((dst_end - dst_pos) < 2))
					break;
				dst_pos[0] = '%';
				dst_pos[1] = *format_pos;
				dst_pos += 2;
				break;
			}
		} else {
			if (od_unlikely((dst_end - dst_pos) < 1))
				break;
			dst_pos[0] = *format_pos;
			dst_pos += 1;
		}
		format_pos++;
	}
	if (od_unlikely((dst_end - dst_pos) < 1))
		return -1;
	dst_pos[0] = 0;
	dst_pos++;
	return dst_pos - output;
}
