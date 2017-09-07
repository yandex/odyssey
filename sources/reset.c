
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
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/log_file.h"
#include "sources/log_system.h"
#include "sources/logger.h"
#include "sources/daemon.h"
#include "sources/scheme.h"
#include "sources/scheme_mgr.h"
#include "sources/config.h"
#include "sources/msg.h"
#include "sources/system.h"
#include "sources/instance.h"
#include "sources/server.h"
#include "sources/server_pool.h"
#include "sources/client.h"
#include "sources/client_pool.h"
#include "sources/route_id.h"
#include "sources/route.h"
#include "sources/route_pool.h"
#include "sources/io.h"
#include "sources/router.h"
#include "sources/pooler.h"
#include "sources/relay.h"
#include "sources/frontend.h"
#include "sources/backend.h"
#include "sources/reset.h"
#include "sources/auth.h"
#include "sources/tls.h"
#include "sources/cancel.h"

int od_reset(od_server_t *server)
{
	od_instance_t *instance = server->system->instance;
	od_route_t *route = server->route;

	/* server left in copy mode */
	if (server->is_copy) {
		od_debug_server(&instance->logger, &server->id, "reset",
		                "in copy, closing");
		goto drop;
	}

	/* support route rollback off */
	if (! route->scheme->pool_rollback) {
		if (server->is_transaction) {
			od_debug_server(&instance->logger, &server->id, "reset",
			                "in active transaction, closing");
			goto drop;
		}
	}

	/* support route cancel off */
	if (! route->scheme->pool_cancel) {
		if (! od_server_sync_is(server)) {
			od_debug_server(&instance->logger, &server->id, "reset",
			                "not synchronized, closing");
			goto drop;
		}
	}

	/* Server is not synchronized.
	 *
	 * Number of queries sent to server is not equal
	 * to the number of received replies. Do the following
	 * logic until server becomes synchronized:
	 *
	 * 1. Wait each ReadyForQuery until we receive all
	 *    replies with 1 sec timeout.
	 *
	 * 2. Send Cancel in other connection.
	 *
	 *    It is possible that client could previously
	 *    pipeline server with requests. Each request
	 *    may stall database on its own way and may require
	 *    additional Cancel request.
	 *
	 * 3. Continue with (1)
	 */
	int wait_timeout = 1000;
	int wait_try = 0;
	int wait_try_cancel = 0;
	int wait_cancel_limit = 1;
	int rc = 0;
	for (;;) {
		while (! od_server_sync_is(server)) {
			od_debug_server(&instance->logger, &server->id, "reset",
			                "not synchronized, wait for %d msec (#%d)",
			                wait_timeout,
			                wait_try);
			wait_try++;
			rc = od_backend_ready_wait(server, NULL, "reset", wait_timeout);
			if (rc == -1)
				break;
		}
		if (rc == -1) {
			if (! machine_timedout())
				goto error;
			if (wait_try_cancel == wait_cancel_limit) {
				od_debug_server(&instance->logger, &server->id, "reset",
				                "server cancel limit reached, closing");
				goto error;
			}
			od_debug_server(&instance->logger, &server->id, "reset",
			                "not responded, cancel (#%d)",
			                wait_try_cancel);
			wait_try_cancel++;
			rc = od_cancel(server->system,
			               route->scheme->storage, &server->key,
			               &server->id);
			if (rc == -1)
				goto error;
			continue;
		}
		assert(od_server_sync_is(server));
		break;
	}
	od_debug_server(&instance->logger, &server->id, "reset",
	                "synchronized");

	/* send rollback in case server has an active
	 * transaction running */
	if (route->scheme->pool_rollback) {
		if (server->is_transaction) {
			char query_rlb[] = "ROLLBACK";
			rc = od_backend_query(server, NULL, "reset rollback", query_rlb,
			                      sizeof(query_rlb));
			if (rc == -1)
				goto error;
			assert(! server->is_transaction);
		}
	}

	/* ready to use (yet maybe discard is required) */
	return  1;
drop:
	return  0;
error:
	return -1;
}

int od_reset_configure(od_server_t *server, shapito_stream_t *params,
                       shapito_be_startup_t *startup)
{
	od_instance_t *instance = server->system->instance;
	od_route_t *route = server->route;
	char query_configure[1024];
	int  size = 0;
	size += snprintf(query_configure + size,
	                 sizeof(query_configure) - size,
	                 "SET client_encoding=%s;SET TimeZone=%s;SET DateStyle=%s;",
	                 route->scheme->client_encoding,
	                 route->scheme->timezone,
	                 route->scheme->datestyle);
	shapito_parameter_t *param;
	shapito_parameter_t *end;
	param = (shapito_parameter_t*)startup->params.buf.start;
	end = (shapito_parameter_t*)startup->params.buf.pos;
	for (; param < end; param = shapito_parameter_next(param)) {
		if (param == startup->user ||
		    param == startup->database)
			continue;
		size += snprintf(query_configure + size,
		                 sizeof(query_configure) - size,
		                 "SET %s='%s';",
		                 shapito_parameter_name(param),
		                 shapito_parameter_value(param));
	}
	if (size == 0)
		return 0;
	od_debug_server(&instance->logger, &server->id, "configure",
	                "%s", query_configure);
	int rc;
	rc = od_backend_query(server, params, "configure", query_configure,
	                      size + 1);
	return rc;
}

int od_reset_discard(od_server_t *server, shapito_stream_t *params)
{
	od_instance_t *instance = server->system->instance;
	char query_discard[] = "DISCARD ALL";
	od_debug_server(&instance->logger, &server->id, "discard",
	                "%s", query_discard);
	return od_backend_query(server, params, "reset", query_discard,
	                        sizeof(query_discard));
}
