
/*
 * Odyssey.
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
#include "sources/config.h"
#include "sources/config_mgr.h"
#include "sources/config_reader.h"
#include "sources/msg.h"
#include "sources/global.h"
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
#include "sources/tls.h"
#include "sources/cancel.h"

int od_reset(od_server_t *server, shapito_stream_t *stream)
{
	od_instance_t *instance = server->global->instance;
	od_route_t *route = server->route;

	/* server left in copy mode */
	if (server->is_copy) {
		od_log(&instance->logger, "reset", server->client, server,
		       "in copy, closing");
		goto drop;
	}

	/* support route rollback off */
	if (! route->config->pool_rollback) {
		if (server->is_transaction) {
			od_log(&instance->logger, "reset", server->client, server,
			       "in active transaction, closing");
			goto drop;
		}
	}

	/* support route cancel off */
	if (! route->config->pool_cancel) {
		if (! od_server_sync_is(server)) {
			od_log(&instance->logger, "reset", server->client, server,
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
			od_log(&instance->logger, "reset", server->client, server,
			       "not synchronized, wait for %d msec (#%d)",
			       wait_timeout,
			       wait_try);
			wait_try++;
			rc = od_backend_ready_wait(server, stream, "reset", 1, wait_timeout);
			if (rc == -1)
				break;
		}
		if (rc == -1) {
			if (! machine_timedout())
				goto error;
			if (wait_try_cancel == wait_cancel_limit) {
				od_error(&instance->logger, "reset", server->client, server,
				         "server cancel limit reached, closing");
				goto error;
			}
			od_log(&instance->logger, "reset", server->client, server,
			       "not responded, cancel (#%d)",
			       wait_try_cancel);
			wait_try_cancel++;
			rc = od_cancel(server->global,
			               stream,
			               route->config->storage, &server->key,
			               &server->id);
			if (rc == -1)
				goto error;
			continue;
		}
		assert(od_server_sync_is(server));
		break;
	}
	od_debug(&instance->logger, "reset", server->client, server,
	         "synchronized");

	/* send rollback in case server has an active
	 * transaction running */
	if (route->config->pool_rollback) {
		if (server->is_transaction) {
			char query_rlb[] = "ROLLBACK";
			rc = od_backend_query(server, stream, "reset rollback", query_rlb,
			                      sizeof(query_rlb));
			if (rc == -1)
				goto error;
			assert(! server->is_transaction);
		}
	}

	/* ready */
	return  1;
drop:
	return  0;
error:
	return -1;
}
