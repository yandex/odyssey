#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 *
 * tls_thread_pool.h: thread pool for offloading TLS handshakes from worker
 * coroutines to dedicated threads, preventing CPU-bound crypto operations
 * (ECDHE key exchange, certificate verification) from blocking the cooperative
 * event loop.
 */

#include <thread_pool.h>
#include <machinarium/io.h>

typedef struct {
	mm_io_t *io;
	machine_tls_t *tls;
	uint32_t timeout;
	int rc;
	char error_msg[128];
} od_tls_handshake_arg_t;

int od_tls_thread_pool_init(size_t count);
void od_tls_thread_pool_destroy(void);

/*
 * Returns 1 if the TLS thread pool is initialized and available for
 * offloading, 0 otherwise (e.g. tls_workers is set to 0 in config).
 */
int od_tls_thread_pool_enabled(void);

/*
 * Offload a TLS handshake to the thread pool.
 *
 * The caller must NOT have the io attached to its own event loop at the time
 * of calling — this function detaches, submits the handshake to a pool worker
 * (which attaches the io to its own event loop, performs the handshake, and
 * detaches), waits for completion, and re-attaches the io to the caller's
 * event loop.
 *
 * Returns 0 on success, -1 on error (including pool queue full).
 */
int od_tls_handshake_offload(mm_io_t *io, machine_tls_t *tls,
			     uint32_t timeout);
