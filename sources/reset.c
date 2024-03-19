
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

int od_reset(od_server_t *server)
{
	od_instance_t *instance = server->global->instance;
	od_route_t *route = server->route;

	/* server left in copy mode
	 * check that number of received CopyIn/CopyOut Responses 
	 * is equal to number received CopyDone msgs.
	 * it is indeed very strange situation if this numbers diffence
	 * is more that 1 (in absolute value).
	 *
	 * However, during client relay step this diffence may be negative,
	 * if msg pipelining is used by driver.
	 * Else drop connection, to avoid complexness of state maintenance
	 */
	if (server->in_out_response_received !=
	    server->done_fail_response_received) {
		od_log(&instance->logger, "reset", server->client, server,
		       "server left in copy, closing and drop connection");
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
	od_retcode_t rc = 0;
	for (;;) {
		/* check that msg syncronization is not broken*/
		if (server->relay.packet > 0)
			goto error;

		while (!od_server_synchronized(server)) {
			od_debug(&instance->logger, "reset", server->client,
				 server,
				 "not synchronized, wait for %d msec (#%d)",
				 wait_timeout, wait_try);
			wait_try++;
			rc = od_backend_ready_wait(server, "reset", 1,
						   wait_timeout,
						   1 /*ignore server errors*/);
			if (rc == NOT_OK_RESPONSE)
				break;
		}
		if (rc == NOT_OK_RESPONSE) {
			if (!machine_timedout())
				goto error;

			/* support route cancel off */
			if (!route->rule->pool->cancel) {
				od_log(&instance->logger, "reset",
				       server->client, server,
				       "not synchronized, closing");
				goto drop;
			}

			if (wait_try_cancel == wait_cancel_limit) {
				od_error(
					&instance->logger, "reset",
					server->client, server,
					"server cancel limit reached, closing");
				goto error;
			}
			od_log(&instance->logger, "reset", server->client,
			       server, "not responded, cancel (#%d)",
			       wait_try_cancel);
			wait_try_cancel++;
			rc = od_cancel(server->global, route->rule->storage,
				       &server->key, &server->id);
			if (rc == NOT_OK_RESPONSE)
				goto error;
			continue;
		}
		assert(od_server_synchronized(server));
		break;
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

	if (od_backend_request_sync_point(server) == NOT_OK_RESPONSE) {
		goto error;
	}

	od_debug(&instance->logger, "reset", server->client, server,
		 "synchronized");

	/* send rollback in case server has an active
	 * transaction running */
	if (route->rule->pool->rollback) {
		if (server->is_transaction) {
			char query_rlb[] = "ROLLBACK";
			rc = od_backend_query(
				server, "reset-rollback", query_rlb, NULL,
				sizeof(query_rlb), wait_timeout, 1,
				0 /*do not ignore server error messages*/);
			if (rc == NOT_OK_RESPONSE)
				goto error;
			assert(!server->is_transaction);
		}
	}

	/* send DISCARD ALL */
	if (route->rule->pool->discard) {
		char query_discard[] = "DISCARD ALL";
		rc = od_backend_query(
			server, "reset-discard", query_discard, NULL,
			sizeof(query_discard), wait_timeout, 1,
			0 /*do not ignore server error messages*/);
		if (rc == NOT_OK_RESPONSE)
			goto error;
	}

	/* send smart discard */
	if (route->rule->pool->smart_discard &&
	    route->rule->pool->discard_query == NULL) {
		char query_discard[] =
			"SET SESSION AUTHORIZATION DEFAULT;RESET ALL;CLOSE ALL;UNLISTEN *;SELECT pg_advisory_unlock_all();DISCARD PLANS;DISCARD SEQUENCES;DISCARD TEMP;";
		rc = od_backend_query(
			server, "reset-discard-smart", query_discard, NULL,
			sizeof(query_discard), wait_timeout, 1,
			0 /*do not ignore server error messages*/);
		if (rc == NOT_OK_RESPONSE)
			goto error;
	}
	if (route->rule->pool->discard_query != NULL) {
		rc = od_backend_query(
			server, "reset-discard-smart-string",
			route->rule->pool->discard_query, NULL,
			strlen(route->rule->pool->discard_query) + 1,
			wait_timeout, 1,
			0 /*do not ignore server error messages*/);
		if (rc == NOT_OK_RESPONSE)
			goto error;
	}

	/* ready */
	return 1;
drop:
	return 0;
error:
	return -1;
}
