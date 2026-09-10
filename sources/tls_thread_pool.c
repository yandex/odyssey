/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 *
 * tls_thread_pool.c: thread pool for offloading TLS handshakes from worker
 * coroutines to dedicated threads.
 */

#include <odyssey.h>

#include <machinarium/machine.h>
#include <machinarium/io.h>

#include <string.h>

#include <thread_pool.h>
#include <tls_thread_pool.h>

static od_thread_pool_t tls_pool;
static int tls_pool_initialized = 0;

int od_tls_thread_pool_init(size_t count)
{
	if (count == 0) {
		tls_pool_initialized = 0;
		return 0;
	}

	int rc = od_thread_pool_init(&tls_pool, "tls", count, 100);
	if (rc != 0) {
		return rc;
	}

	tls_pool_initialized = 1;
	return 0;
}

void od_tls_thread_pool_destroy(void)
{
	if (!tls_pool_initialized) {
		return;
	}

	od_thread_pool_destroy(&tls_pool);
	tls_pool_initialized = 0;
}

int od_tls_thread_pool_enabled(void)
{
	return tls_pool_initialized;
}

static void *od_tls_handshake_task(void *arg)
{
	od_tls_handshake_arg_t *task = (od_tls_handshake_arg_t *)arg;
	mm_io_t *io = task->io;

	mm_errno_set(0);

	if (mm_io_attach(io) == -1) {
		task->rc = -1;
		snprintf(task->error_msg, sizeof(task->error_msg),
			 "failed to attach io to pool worker");
		return arg;
	}

	int rc = mm_io_set_tls(io, task->tls, task->timeout);
	task->rc = rc;

	if (rc == -1) {
		char *err = mm_io_error(io);
		if (err) {
			snprintf(task->error_msg, sizeof(task->error_msg),
				 "%s", err);
		} else {
			snprintf(task->error_msg, sizeof(task->error_msg),
				 "tls handshake failed");
		}
	}

	mm_io_detach(io);

	return arg;
}

int od_tls_handshake_offload(mm_io_t *io, machine_tls_t *tls,
			     uint32_t timeout)
{
	if (!tls_pool_initialized) {
		mm_errno_set(EINPROGRESS);
		return -1;
	}

	if (io->attached) {
		if (mm_io_detach(io) == -1) {
			return -1;
		}
	}

	od_tls_handshake_arg_t arg;
	memset(&arg, 0, sizeof(arg));
	arg.io = io;
	arg.tls = tls;
	arg.timeout = timeout;

	od_future_t *future = od_thread_pool_submit(&tls_pool,
						     od_tls_handshake_task,
						     &arg, NULL, NULL, 0);
	if (future == NULL) {
		mm_io_attach(io);
		return -1;
	}

	int rc = od_thread_pool_wait(future, UINT32_MAX);
	od_future_unref(future);

	if (rc != 0) {
		mm_io_attach(io);
		return -1;
	}

	if (mm_io_attach(io) == -1) {
		return -1;
	}

	return arg.rc;
}
