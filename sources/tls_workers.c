/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 *
 * tls_workers.c: dedicated machines for offloading client TLS handshakes.
 */

#include <odyssey.h>

#include <stdatomic.h>

#include <machinarium/machinarium.h>
#include <machinarium/machine.h>
#include <machinarium/io.h>

#include <util.h>
#include <od_memory.h>
#include <tls_workers.h>

typedef struct {
	mm_io_t *io;
	machine_tls_t *tls;
	uint32_t timeout;
	uint64_t deadline;
	int rc;
	int error;
} od_tls_handshake_req_t;

typedef struct {
	int id;
	int64_t machine;
	machine_wait_flag_t *stop;
} od_tls_worker_t;

static od_tls_worker_t *tls_workers = NULL;
static size_t tls_workers_count = 0;
static atomic_uint_fast64_t tls_workers_next = 0;

static void od_tls_handshake(void *arg)
{
	od_tls_handshake_req_t *req = arg;
	if (machine_cancelled()) {
		req->rc = -1;
		req->error = ECANCELED;
		return;
	}
	if (req->timeout != UINT32_MAX) {
		mm_clock_reset(&mm_self->loop.clock);
		uint64_t now = machine_time_ms();
		if (now >= req->deadline) {
			req->rc = -1;
			req->error = ETIMEDOUT;
			return;
		}
		req->timeout = req->deadline - now;
	}

	req->rc = mm_io_attach(req->io);
	if (req->rc == 0) {
		req->rc = mm_io_set_tls(req->io, req->tls, req->timeout);
		req->error = mm_errno_get();
		if (mm_io_detach(req->io) == -1) {
			req->rc = -1;
			req->error = mm_errno_get();
			/* Closing also removes the descriptor from this event loop. */
			mm_io_close(req->io);
			req->io->attached = 0;
		}
	} else {
		req->error = mm_errno_get();
	}
}

static void od_tls_worker_main(void *arg)
{
	od_tls_worker_t *worker = arg;

	/* One lifetime coroutine per worker; handshakes run on visiting frontend
	 * stacks. No allocation is needed to signal shutdown. */
	machine_wait_flag_wait(worker->stop, UINT32_MAX);
}

int od_tls_workers_init(size_t count)
{
	if (count == 0) {
		return 0;
	}

	tls_workers = od_malloc(sizeof(od_tls_worker_t) * count);
	if (tls_workers == NULL) {
		mm_errno_set(ENOMEM);
		return -1;
	}
	memset(tls_workers, 0, sizeof(od_tls_worker_t) * count);

	for (size_t i = 0; i < count; ++i) {
		od_tls_worker_t *worker = &tls_workers[i];
		worker->id = (int)i;
		worker->stop = machine_wait_flag_create();
		if (worker->stop == NULL) {
			goto error;
		}

		char name[32];
		od_snprintf(name, sizeof(name), "tls: %d", worker->id);
		worker->machine =
			machine_create(name, od_tls_worker_main, worker);
		if (worker->machine == -1) {
			machine_wait_flag_destroy(worker->stop);
			goto error;
		}

		tls_workers_count++;
	}

	return 0;

error:
	od_tls_workers_destroy();
	return -1;
}

void od_tls_workers_destroy(void)
{
	for (size_t i = 0; i < tls_workers_count; ++i) {
		machine_wait_flag_set(tls_workers[i].stop);
	}

	for (size_t i = 0; i < tls_workers_count; ++i) {
		machine_wait_nb(tls_workers[i].machine);
		machine_wait_flag_destroy(tls_workers[i].stop);
	}

	od_free(tls_workers);
	tls_workers = NULL;
	tls_workers_count = 0;
}

int od_tls_workers_enabled(void)
{
	return tls_workers_count > 0;
}

int od_tls_workers_handshake(mm_io_t *io, machine_tls_t *tls, uint32_t timeout)
{
	od_tls_handshake_req_t req;
	memset(&req, 0, sizeof(req));
	req.io = io;
	req.tls = tls;
	req.timeout = timeout;
	if (timeout != UINT32_MAX) {
		mm_clock_reset(&mm_self->loop.clock);
		req.deadline = machine_time_ms() + timeout;
	}

	if (mm_io_detach(io) == -1) {
		return -1;
	}

	uint64_t n = atomic_fetch_add(&tls_workers_next, 1);
	od_tls_worker_t *worker = &tls_workers[n % tls_workers_count];
	if (machine_coroutine_call_on(worker->machine, od_tls_handshake,
				      &req) == -1) {
		req.rc = -1;
		req.error = mm_errno_get();
	}

	if (io->fd != -1 && mm_io_attach(io) == -1) {
		return -1;
	}
	if (machine_cancelled()) {
		req.rc = -1;
		req.error = ECANCELED;
	}

	mm_errno_set(req.error);
	return req.rc;
}
