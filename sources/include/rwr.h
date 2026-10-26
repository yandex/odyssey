#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <types.h>
#include <client.h>

/*
 * Read-your-writes consistency (SET odyssey.rwr_consistent = on):
 *
 * After each client write executed on the primary, use ttt.last_write_lsn
 * feedback or capture pg_current_wal_insert_lsn() if no feedback arrived.
 * Before a query on a replica, send WAIT FOR LSN '<lsn>' so the read
 * observes all the writes done so far.
 *
 * Both the capture and the wait are bounded by the per-storage
 * rwr_wait_timeout option (rwr_wait_timeout_ms; 0 = no limit): a side
 * query that does not fit the budget makes the backend connection
 * unusable and is reported to the relay.
 *
 * Only the simple protocol query path is supported.
 */

/*
 * Capture LSN on the primary after a client write. Returns
 * NOT_OK_RESPONSE only if the server connection is broken and cannot be
 * reused.
 */
od_retcode_t od_rwr_capture_lsn(od_client_t *client, od_server_t *server);

/* Accept feedback from the current client response, not startup/deploy. */
void od_rwr_receive_lsn(od_server_t *server, const char *value, size_t len);

/*
 * Before forwarding a read query to a replica, wait for the previously
 * captured LSN. Returns NOT_OK_RESPONSE only if the server connection is
 * broken and cannot be reused; on a regular WAIT error the query is
 * processed without waiting.
 */
od_retcode_t od_rwr_wait_lsn(od_client_t *client, od_server_t *server);
