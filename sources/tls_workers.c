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
	int rc;
	int error;
	machine_wait_flag_t *done;
} od_tls_handshake_req_t;

typedef struct {
	int id;
	int64_t machine;
	machine_channel_t *channel;
} od_tls_worker_t;

static od_tls_worker_t *tls_workers = NULL;
static size_t tls_workers_count = 0;
static atomic_uint_fast64_t tls_workers_next = 0;

static void od_tls_handshake_coroutine(void *arg)
{
	od_tls_handshake_req_t *req = arg;

	req->rc = mm_io_attach(req->io);
	if (req->rc == 0) {
		req->rc = mm_io_set_tls(req->io, req->tls, req->timeout);
		req->error = mm_errno_get();
		mm_io_detach(req->io);
	} else {
		req->error = mm_errno_get();
	}

	/* req lives on the waiter's stack, do not touch it after this */
	machine_wait_flag_set(req->done);
}

static void od_tls_worker_main(void *arg)
{
	od_tls_worker_t *worker = arg;

	for (;;) {
		machine_msg_t *msg;
		msg = machine_channel_read(worker->channel, 10 * 1000);
		if (msg == NULL) {
			continue;
		}

		od_tls_handshake_req_t *req;
		req = *(od_tls_handshake_req_t **)machine_msg_data(msg);
		machine_msg_free(msg);

		if (req == NULL) {
			/* shutdown, in-flight handshakes are finished by
			 * their own coroutines before machine exits */
			break;
		}

		int64_t id = machine_coroutine_create(
			od_tls_handshake_coroutine, req);
		if (id == -1) {
			req->rc = -1;
			req->error = mm_errno_get();
			machine_wait_flag_set(req->done);
		}
	}
}

static int od_tls_worker_send(od_tls_worker_t *worker,
			      od_tls_handshake_req_t *req)
{
	machine_msg_t *msg;
	msg = machine_msg_create(sizeof(od_tls_handshake_req_t *));
	if (msg == NULL) {
		return -1;
	}
	*(od_tls_handshake_req_t **)machine_msg_data(msg) = req;
	machine_channel_write(worker->channel, msg);
	return 0;
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
		worker->channel = machine_channel_create();
		if (worker->channel == NULL) {
			goto error;
		}

		char name[32];
		od_snprintf(name, sizeof(name), "tls: %d", worker->id);
		worker->machine =
			machine_create(name, od_tls_worker_main, worker);
		if (worker->machine == -1) {
			machine_channel_free(worker->channel);
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
		od_tls_worker_send(&tls_workers[i], NULL);
	}

	for (size_t i = 0; i < tls_workers_count; ++i) {
		machine_wait(tls_workers[i].machine);
		machine_channel_free(tls_workers[i].channel);
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
	req.done = machine_wait_flag_create();
	if (req.done == NULL) {
		goto inplace;
	}

	if (mm_io_detach(io) == -1) {
		machine_wait_flag_destroy(req.done);
		return -1;
	}

	uint64_t n = atomic_fetch_add(&tls_workers_next, 1);
	od_tls_worker_t *worker = &tls_workers[n % tls_workers_count];
	if (od_tls_worker_send(worker, &req) == -1) {
		machine_wait_flag_destroy(req.done);
		if (mm_io_attach(io) == -1) {
			return -1;
		}
		goto inplace;
	}

	/*
	 * the handshake is bounded by timeout, but io and req must not
	 * be released until the worker is done with them anyway
	 */
	while (machine_wait_flag_wait(req.done, UINT32_MAX) != 0) {
	}
	machine_wait_flag_destroy(req.done);

	if (mm_io_attach(io) == -1) {
		return -1;
	}

	mm_errno_set(req.error);
	return req.rc;

inplace:
	return mm_io_set_tls(io, tls, timeout);
}
