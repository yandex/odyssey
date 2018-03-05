
/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
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
#include "sources/scheme.h"
#include "sources/scheme_mgr.h"
#include "sources/config_reader.h"
#include "sources/msg.h"
#include "sources/system.h"
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
#include "sources/pooler.h"
#include "sources/worker.h"
#include "sources/frontend.h"
#include "sources/backend.h"
#include "sources/reset.h"
#include "sources/auth.h"
#include "sources/auth_query.h"

static inline int
od_auth_query_do(od_server_t *server, shapito_stream_t *stream,
                 char *query, int len,
                 shapito_password_t *result)
{
	od_instance_t *instance = server->system->instance;

	od_debug(&instance->logger, "auth_query", server->client, server,
	         "%s", query);
	int rc;
	shapito_stream_reset(stream);
	rc = shapito_fe_write_query(stream, query, len);
	if (rc == -1)
		return -1;
	rc = od_write(server->io, stream);
	if (rc == -1) {
		od_error(&instance->logger, "auth_query", server->client, server,
		         "write error: %s",
		         machine_error(server->io));
		return -1;
	}

	/* update server sync state and stats */
	od_server_stat_request(server, 1);

	/* wait for response */
	int has_result = 0;
	while (1) {
		shapito_stream_reset(stream);
		int rc;
		rc = od_read(server->io, stream, UINT32_MAX);
		if (rc == -1) {
			if (! machine_timedout()) {
				od_error(&instance->logger, "auth_query", server->client, server,
				         "read error: %s",
				         machine_error(server->io));
			}
			return -1;
		}
		shapito_be_msg_t type = *stream->start;
		od_debug(&instance->logger, "auth_query", server->client, server, "%s",
		         shapito_be_msg_to_string(type));

		switch (type) {
		case SHAPITO_BE_ERROR_RESPONSE:
			od_backend_error(server, "auth_query", stream->start,
			                 shapito_stream_used(stream));
			return -1;
		case SHAPITO_BE_ROW_DESCRIPTION:
			break;
		case SHAPITO_BE_DATA_ROW:
		{
			if (has_result) {
				return -1;
			}
			char *pos = stream->start + 1;
			uint32_t pos_size = shapito_stream_used(stream) - 1;

			/* size */
			uint32_t size;
			rc = shapito_stream_read32(&size, &pos, &pos_size);
			if (shapito_unlikely(rc == -1)) {
				return -1;
			}
			/* count */
			uint16_t count;
			rc = shapito_stream_read16(&count, &pos, &pos_size);
			if (shapito_unlikely(rc == -1)) {
				return -1;
			}
			if (count != 2) {
				return -1;
			}

			/* user (not used) */
			uint32_t user_len;
			rc = shapito_stream_read32(&user_len, &pos, &pos_size);
			if (shapito_unlikely(rc == -1)) {
				return -1;
			}
			char *user = pos;
			rc = shapito_stream_read(user_len, &pos, &pos_size);
			if (shapito_unlikely(rc == -1)) {
				return -1;
			}
			(void)user;
			(void)user_len;

			/* password */
			uint32_t password_len;
			rc = shapito_stream_read32(&password_len, &pos, &pos_size);
			if (shapito_unlikely(rc == -1)) {
				return -1;
			}
			char *password = pos;
			rc = shapito_stream_read(password_len, &pos, &pos_size);
			if (shapito_unlikely(rc == -1)) {
				return -1;
			}

			result->password = malloc(password_len + 1);
			if (result->password == NULL)
				return -1;
			memcpy(result->password, password, password_len);
			result->password[password_len] = 0;
			result->password_len = password_len + 1;

			has_result = 1;
			break;
		}
		case SHAPITO_BE_READY_FOR_QUERY:
			od_backend_ready(server, "auth_query", stream->start,
			                 shapito_stream_used(stream));
			return 0;
		default:
			break;
		}
	}
	return 0;
}

__attribute__((hot)) static inline int
od_auth_query_format(od_schemeroute_t *scheme, shapito_parameter_t *user,
                     char *output, int output_len)
{
	char *dst_pos = output;
	char *dst_end = output + output_len;
	char *format_pos = scheme->auth_query;
	char *format_end = scheme->auth_query + strlen(scheme->auth_query);
	while (format_pos < format_end)
	{
		if (*format_pos == '%') {
			format_pos++;
			if (od_unlikely(format_pos == format_end))
				break;
			if (*format_pos == 'u') {
				int len;
				len = od_snprintf(dst_pos, dst_end - dst_pos, "%s",
				                  shapito_parameter_value(user));
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

int od_auth_query(od_system_t *system,
                  shapito_stream_t *stream,
                  od_schemeroute_t *scheme,
                  shapito_parameter_t *user,
                  shapito_password_t *password)
{
	od_instance_t *instance = system->instance;

	/* create internal auth client */
	od_client_t *auth_client;
	auth_client = od_client_allocate();
	if (auth_client == NULL)
		return -1;
	auth_client->system = system;

	od_idmgr_generate(&instance->id_mgr, &auth_client->id, "a");

	/* set auth query route db and user */
	shapito_parameters_add(&auth_client->startup.params, "database", 9,
	                       scheme->auth_query_db,
	                       strlen(scheme->auth_query_db) + 1);

	shapito_parameters_add(&auth_client->startup.params, "user", 5,
	                       scheme->auth_query_user,
	                       strlen(scheme->auth_query_user) + 1);

	shapito_parameter_t *param;
	param = (shapito_parameter_t*)auth_client->startup.params.buf.start;
	auth_client->startup.database = param;

	param = shapito_parameter_next(param);
	auth_client->startup.user = param;

	/* route */
	od_routerstatus_t status;
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
		rc = od_backend_connect(server, stream, "auth_query");
		if (rc == -1) {
			od_router_close_and_unroute(auth_client);
			od_client_free(auth_client);
			return -1;
		}
	}

	/* preformat and execute query */
	char query[512];
	int  query_len;
	query_len = od_auth_query_format(scheme, user, query, sizeof(query));

	rc = od_auth_query_do(server, stream, query, query_len, password);
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
