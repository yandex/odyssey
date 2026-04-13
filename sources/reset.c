
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>

#include <reset.h>
#include <server.h>
#include <route.h>
#include <backend.h>
#include <instance.h>
#include <global.h>
#include <query.h>
#include <cancel.h>
#include <stream.h>

int od_reset(od_server_t *server)
{
	od_instance_t *instance = server->global->instance;
	od_route_t *route = server->route;

	if (server->oom) {
		od_log(&instance->logger, "reset", server->client, server,
		       "server in oom mode, closing and drop connection");
		goto drop;
	}

	/* server left in copy mode
	 * check that number of received CopyIn/CopyOut Responses 
	 * is equal to number received CopyDone msgs.
	 * it is indeed very strange situation if this numbers difference
	 * is more that 1 (in absolute value).
	 */
	if (server->copy_mode) {
		od_log(&instance->logger, "reset", server->client, server,
		       "server left in copy, closing and drop connection");
		goto drop;
	}

	if (server->msg_broken) {
		od_log(&instance->logger, "reset", server->client, server,
		       "server left with partly-read message, closing and drop connection");
		goto drop;
	}

	/* support route rollback off */
	if (!route->rule->pool->rollback) {
		if (server->is_transaction) {
			od_log(&instance->logger, "reset", server->client,
			       server, "in active transaction, closing");
			goto drop;
		}
	}

	uint32_t reset_timeout_ms = UINT32_MAX;
	if (route->rule->pool->reset_timeout_ms > 0 &&
	    route->rule->pool->reset_timeout_ms < UINT32_MAX) {
		reset_timeout_ms =
			(uint32_t)route->rule->pool->reset_timeout_ms;
	}

	/*
	 * Server is not synchronized.
	 *
	 * Number of queries sent to server is not equal
	 * to the number of received replies.
	 * Send cancel and try wait for synchronized state
	 */
	if (!od_server_synchronized(server)) {
		if (!route->rule->pool->cancel) {
			od_log(&instance->logger, "reset", server->client,
			       server, "server left in query, closing");
			goto drop;
		}

		/*
		 * if query already completed on backend
		 * then cancel will do nothing, so it is ok to send
		 * cancel unconditionally
		 */
		int rc = od_cancel(server->global, route->rule->storage,
				   od_server_pool_address(server), &server->key,
				   &server->id);
		if (rc == NOT_OK_RESPONSE) {
			goto error;
		}

		od_frontend_status_t st = od_service_stream_server_until_rfq(
			"reset", server, 1 /* ignore_errors */,
			reset_timeout_ms);
		if (st != OD_OK) {
			od_log(&instance->logger, "reset", server->client,
			       server,
			       "rfq waiting failed - closing, status = %d (%s)",
			       st, od_frontend_status_to_str(st));
			goto drop;
		}

		/* still not synchronized - give up */
		if (!od_server_synchronized(server)) {
			od_log(&instance->logger, "reset", server->client,
			       server,
			       "sync failed, server left in query (sync_reply = %d, sync_request = %d) - closing",
			       server->sync_reply, server->sync_request);
			goto drop;
		}
	}

	/* Request one more sync point here.
	* In `od_server_synchronized` we
	* count number of sync/query msg send to connection
	* and number of RFQ received, if this numbers are equal,  
	* we decide server connection as sync. However, this might be 
	* not true, if client-server relay advanced some extended proto
	* msgs without sync. To safely execute discard queries, we need to
	* advadance sync point first.
	*/

#ifdef FIX_ME_PLEASE
	if (od_backend_request_sync_point(server) == NOT_OK_RESPONSE) {
		goto error;
	}
#endif

	od_debug(&instance->logger, "reset", server->client, server,
		 "synchronized");

	int rc;

	/* send rollback in case server has an active
	 * transaction running */
	if (route->rule->pool->rollback) {
		if (server->is_transaction) {
			char query_rlb[] = "ROLLBACK";
			rc = od_backend_query(server, "reset-rollback",
					      query_rlb, NULL,
					      sizeof(query_rlb),
					      reset_timeout_ms);
			if (rc == NOT_OK_RESPONSE) {
				goto error;
			}
			assert(!server->is_transaction);
		}
	}

	/* send DISCARD ALL */
	if (route->rule->pool->discard) {
		char query_discard[] = "DISCARD ALL";
		rc = od_backend_query(server, "reset-discard", query_discard,
				      NULL, sizeof(query_discard),
				      reset_timeout_ms);
		if (rc == NOT_OK_RESPONSE) {
			goto error;
		}
	}

	/* send smart discard */
	if (route->rule->pool->smart_discard &&
	    route->rule->pool->discard_query == NULL) {
		char query_discard[] =
			"SET SESSION AUTHORIZATION DEFAULT;RESET ALL;CLOSE ALL;UNLISTEN *;SELECT pg_advisory_unlock_all();DISCARD PLANS;DISCARD SEQUENCES;DISCARD TEMP;";
		rc = od_backend_query(server, "reset-discard-smart",
				      query_discard, NULL,
				      sizeof(query_discard), reset_timeout_ms);
		if (rc == NOT_OK_RESPONSE) {
			goto error;
		}
	}
	if (route->rule->pool->discard_query != NULL) {
		rc = od_backend_query(server, "reset-discard-smart-string",
				      route->rule->pool->discard_query, NULL,
				      strlen(route->rule->pool->discard_query) +
					      1,
				      reset_timeout_ms);
		if (rc == NOT_OK_RESPONSE) {
			goto error;
		}
	}

	/* ready */
	return 1;
drop:
	return 0;
error:
	return -1;
}
