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
 * After each client write executed on the primary, capture
 * pg_current_wal_insert_lsn() on the primary. If the next client query is
 * executed on a replica, send WAIT FOR LSN '<lsn>' to the replica before
 * forwarding the client query, so the read observes all the writes done
 * so far.
 *
 * Only the simple protocol query path is supported.
 */

/* both captures and waits are blocked until the server reports infinite */
#define OD_RWR_QUERY_TIMEOUT_MS UINT32_MAX

/*
 * Capture LSN on the primary after a client write. Returns
 * NOT_OK_RESPONSE only if the server connection is broken and cannot be
 * reused.
 */
od_retcode_t od_rwr_capture_lsn(od_client_t *client, od_server_t *server);

/*
 * Before forwarding a read query to a replica, wait for the previously
 * captured LSN. Returns NOT_OK_RESPONSE only if the server connection is
 * broken and cannot be reused; on a regular WAIT error the query is
 * processed without waiting.
 */
od_retcode_t od_rwr_wait_lsn(od_client_t *client, od_server_t *server);
