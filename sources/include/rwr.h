#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <types.h>
#include <client.h>

/*
 * Read-your-writes consistency (SET odyssey.rwr_consistent = on, or
 * startup options = '-c odyssey.rwr_consistent=on'):
 *
 * Before returning a primary connection to the pool in od_reset, capture
 * pg_current_wal_insert_lsn() on the primary. When attaching to a replica,
 * send WAIT FOR LSN '<lsn>' to the replica before
 * forwarding any client commands, so the read observes all the writes done
 * so far.
 *
 * Both the capture and the wait are bounded by rwr_wait_timeout
 * (milliseconds; 0 = no limit). A rule overrides the global value. A side
 * query that does not fit the budget makes the backend connection
 * unusable and is reported to the relay.
 *
 * Capture and wait are independent of SQL parsing and support both
 * simple and extended query protocols.
 */

/*
 * Capture LSN on the primary after backend cleanup. Returns
 * NOT_OK_RESPONSE only if the server connection is broken and cannot be
 * reused.
 */
od_retcode_t od_rwr_capture_lsn(od_client_t *client, od_server_t *server);

/*
 * Before forwarding commands to a newly attached replica, wait for the
 * captured LSN. Returns NOT_OK_RESPONSE only if the server connection is
 * broken and cannot be reused; on a regular WAIT error the query is
 * processed without waiting.
 */
od_retcode_t od_rwr_wait_lsn(od_client_t *client, od_server_t *server);
