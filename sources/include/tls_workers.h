#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 *
 * tls_workers.h: dedicated machines for offloading client TLS handshakes,
 * so CPU-bound crypto (key exchange, certificate verification) does not
 * block worker event loops.
 */

#include <machinarium/machinarium.h>
#include <machinarium/io.h>

int od_tls_workers_init(size_t count);
void od_tls_workers_destroy(void);

/*
 * Returns 1 if TLS workers are started (tls_workers > 0 in config),
 * 0 otherwise.
 */
int od_tls_workers_enabled(void);

/*
 * Perform TLS handshake on one of the TLS workers.
 *
 * The io is detached from the caller's event loop, attached to the TLS
 * worker's loop for the handshake and attached back before return.
 * Every handshake runs in its own coroutine, so slow clients do not block
 * each other.
 *
 * Falls back to handshake in the caller's machine if the request can not
 * be submitted.
 *
 * Returns 0 on success, -1 on error (machine_errno() is set).
 */
int od_tls_workers_handshake(mm_io_t *io, machine_tls_t *tls, uint32_t timeout);
