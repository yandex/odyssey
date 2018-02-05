
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
#include "sources/config.h"
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
#include "sources/router.h"
#include "sources/pooler.h"
#include "sources/relay.h"
#include "sources/frontend.h"
#include "sources/backend.h"
#include "sources/deploy.h"
#include "sources/auth.h"
#include "sources/tls.h"
#include "sources/cancel.h"

static inline int
od_deploy_add(od_server_t *server, shapito_parameters_t *params,
              char *query, int size,
              char *name, int name_len)
{
	/* add configureation parameter if it is defined by
	 * client and server is not already configured using exact
	 * parameter value */
	shapito_parameter_t *server_param;
	shapito_parameter_t *client_param;
	client_param = shapito_parameters_find(params, name, name_len);
	if (client_param == NULL)
		return 0;
	server_param = shapito_parameters_find(&server->params, name, name_len);
	if (server_param) {
		if (server_param->value_len == client_param->value_len) {
			if (memcmp(shapito_parameter_value(server_param),
			           shapito_parameter_value(client_param),
			           client_param->value_len) == 0)
				return 0;
		}
	}

	char quote_value[256];
	int rc;
	rc = shapito_parameter_quote(shapito_parameter_value(client_param),
	                             quote_value, sizeof(quote_value));
	if (rc == -1)
		return 0;

	return od_snprintf(query, size, "SET %s=%s;",
	                   shapito_parameter_name(client_param),
	                   quote_value);
}

int od_deploy(od_server_t *server, char *context,
              shapito_parameters_t *params,
              int wait)
{
	od_instance_t *instance = server->system->instance;
	int rc;
	shapito_stream_t *stream = &server->stream;
	shapito_stream_reset(stream);

	int  query_count = 1;
	char query_discard[] = "DISCARD ALL";
	rc = shapito_fe_write_query(stream, query_discard, sizeof(query_discard));
	if (rc == -1)
		return -1;

	char query[512];
	int  size = 0;
	size += od_deploy_add(server, params, query + size, sizeof(query) - size,
	                      "TimeZone", 9);
	size += od_deploy_add(server, params, query + size, sizeof(query) - size,
	                      "DateStyle", 10);
	size += od_deploy_add(server, params, query + size, sizeof(query) - size,
	                      "client_encoding", 16);
	size += od_deploy_add(server, params, query + size, sizeof(query) - size,
	                      "application_name", 17);
	size += od_deploy_add(server, params, query + size, sizeof(query) - size,
	                      "extra_float_digits", 19);
	size += od_deploy_add(server, params, query + size, sizeof(query) - size,
	                      "standard_conforming_strings", 28);
	size += od_deploy_add(server, params, query + size, sizeof(query) - size,
	                      "statement_timeout", 18);
	size += od_deploy_add(server, params, query + size, sizeof(query) - size,
	                      "search_path", 12);
	if (size == 0) {
		od_debug(&instance->logger, context, server->client, server,
		         "%s", "no need to configure");
	} else {
		od_debug(&instance->logger, context, server->client, server,
		         "%s", query);
		size++;
		query_count++;
		rc = shapito_fe_write_query(stream, query, size);
		if (rc == -1)
			return -1;
	}

	rc = od_write(server->io, stream);
	if (rc == -1) {
		od_error(&instance->logger, context, server->client, server,
		         "write error: %s",
		         machine_error(server->io));
		return -1;
	}

	/* update server sync state and stats */
	od_server_stat_request(server, query_count);

	if (! wait)
		return 0;

	/* wait for reply */
	rc = od_backend_ready_wait(server, context, query_count, UINT32_MAX);
	return rc;
}


int od_deploy_write(od_server_t *server, char *context,
                    shapito_stream_t *stream,
                    shapito_parameters_t *params)
{
	od_instance_t *instance = server->system->instance;
	int rc;

	/* discard */
	int  query_count = 1;
	char query_discard[] = "DISCARD ALL";
	rc = shapito_fe_write_query(stream, query_discard, sizeof(query_discard));
	if (rc == -1)
		return -1;

	/* parameters */
	char query[512];
	int  size = 0;
	size += od_deploy_add(server, params, query + size, sizeof(query) - size,
	                      "TimeZone", 9);
	size += od_deploy_add(server, params, query + size, sizeof(query) - size,
	                      "DateStyle", 10);
	size += od_deploy_add(server, params, query + size, sizeof(query) - size,
	                      "client_encoding", 16);
	size += od_deploy_add(server, params, query + size, sizeof(query) - size,
	                      "application_name", 17);
	size += od_deploy_add(server, params, query + size, sizeof(query) - size,
	                      "extra_float_digits", 19);
	size += od_deploy_add(server, params, query + size, sizeof(query) - size,
	                      "standard_conforming_strings", 28);
	size += od_deploy_add(server, params, query + size, sizeof(query) - size,
	                      "statement_timeout", 18);
	size += od_deploy_add(server, params, query + size, sizeof(query) - size,
	                      "search_path", 12);
	if (size == 0) {
		od_debug(&instance->logger, context, server->client, server,
		         "%s", "no need to configure");
	} else {
		od_debug(&instance->logger, context, server->client, server,
		         "%s", query);
		size++;
		query_count++;
		rc = shapito_fe_write_query(stream, query, size);
		if (rc == -1)
			return -1;
	}
	return query_count;
}
