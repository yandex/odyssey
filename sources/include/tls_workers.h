#pragma once

/* Dedicated workers for PostgreSQL SSL negotiation and TLS handshakes. */

#include <machinarium/machinarium.h>

int od_tls_workers_init(size_t count);
int od_tls_workers_enabled(void);

/* Takes ownership of an OD_MSG_CLIENT_NEW message and its client. On success,
 * forwards the same message to the regular worker pool; on error closes the
 * client and releases its routing slot. Call only while workers are enabled. */
void od_tls_workers_feed(machine_msg_t *msg);

/* Called from a coroutine after acceptors have stopped. Drains queued and
 * active negotiations before returning. Regular workers must still be alive. */
void od_tls_workers_destroy(void);
