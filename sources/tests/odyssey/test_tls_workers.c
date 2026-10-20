#include <fcntl.h>
#include <sys/socket.h>
#include <machinarium/machine.h>
#include <machinarium/io.h>
#include <machinarium/mm.h>
#include <tls_workers.h>
#include <tests/odyssey_test.h>

typedef struct {
	mm_io_t *server;
	mm_io_t *client;
	machine_tls_t *tls;
	uint32_t timeout;
	int success;
	int error;
	int done;
	int queued_timeout;
} tls_worker_test_t;

typedef struct {
	uint64_t machine;
	atomic_int blocked;
	atomic_int release;
} tls_queue_test_t;

static void tls_worker_block(void *arg)
{
	tls_queue_test_t *t = arg;
	atomic_store(&t->blocked, 1);
	while (!atomic_load(&t->release)) {
		usleep(1000);
	}
}

static void tls_worker_blocker(void *arg)
{
	tls_queue_test_t *t = arg;
	test(machine_coroutine_call_on(t->machine, tls_worker_block, t) == 0);
}

static mm_io_t *tls_worker_io(int fd, int accepted)
{
	test(fcntl(fd, F_SETFL, O_NONBLOCK) == 0);
	mm_io_t *io = mm_io_create();
	test(io != NULL);
	io->is_unix_socket = 1;
	io->accepted = accepted;
	io->connected = 1;
	test(mm_io_socket_set(io, fd) == 0);
	test(mm_io_attach(io) == 0);
	return io;
}

static void tls_worker_server(void *arg)
{
	tls_worker_test_t *t = arg;
	mm_machine_t *home = mm_self;
	mm_coroutine_t *coroutine = home->scheduler.current;
	int rc = od_tls_workers_handshake(t->server, t->tls, t->timeout);
	test(mm_self == home);
	test(mm_self->scheduler.current == coroutine);
	test(t->server->attached);
	if (t->success) {
		test(rc == 0);
		machine_msg_t *msg = machine_msg_create(0);
		test(msg != NULL);
		test(machine_msg_write(msg, "hello", 5) == 0);
		test(machine_write(t->server, msg, 1000) == 0);
	} else {
		test(rc == -1);
		if (t->queued_timeout) {
			/* Expiry in the incoming queue must skip TLS setup entirely. */
			test(t->server->tls_ssl == NULL);
		}
		if (t->error) {
			test(machine_errno() == t->error);
		}
	}
	test(mm_io_close(t->server) == 0);
	mm_io_free(t->server);
	t->done = 1;
}

static void tls_worker_client(void *arg)
{
	tls_worker_test_t *t = arg;
	int rc = mm_io_set_tls(t->client, t->tls, 1000);
	if (rc == -1) {
		printf("TLS client: %s\n", mm_io_error(t->client));
	}
	test(rc == 0);
	machine_msg_t *msg = machine_read(t->client, 5, 1000);
	test(msg != NULL);
	test(memcmp(machine_msg_data(msg), "hello", 5) == 0);
	machine_msg_free(msg);
	test(mm_io_close(t->client) == 0);
	mm_io_free(t->client);
}

static void tls_worker_tests(void *arg)
{
	(void)arg;
	test(od_tls_workers_init(1) == 0);
	tls_queue_test_t queue = { .machine = UINT64_MAX };
	mm_machinemgr_t *mgr = &machinarium.machine_mgr;
	mm_spinlock_lock(&mgr->lock);
	mm_list_t *entry;
	mm_list_foreach (&mgr->list, entry) {
		mm_machine_t *machine =
			mm_container_of(entry, mm_machine_t, link);
		if (machine->name && strcmp(machine->name, "tls: 0") == 0) {
			queue.machine = machine->id;
		}
	}
	mm_spinlock_unlock(&mgr->lock);
	test(queue.machine != UINT64_MAX);
	machine_tls_t *tls = machine_tls_create();
	test(tls != NULL);
	test(machine_tls_set_verify(tls, "none") == 0);
	test(machine_tls_set_ca_file(tls, "./machinarium/ca.crt") == 0);
	test(machine_tls_set_cert_file(tls, "./machinarium/server.crt") == 0);
	test(machine_tls_set_key_file(tls, "./machinarium/server.key") == 0);

	/* Success, silent peer, cancellation, peer disconnect, queue timeout. */
	for (int mode = 0; mode < 5; mode++) {
		int sockets[2];
		test(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
		tls_worker_test_t t = {
			.server = tls_worker_io(sockets[0], 1),
			.client = tls_worker_io(sockets[1], 0),
			.tls = tls,
			.timeout = mode == 1 || mode == 4 ? 20 : 60000,
			.success = mode == 0,
			.queued_timeout = mode == 4,
			.error = mode == 1 || mode == 4 ? ETIMEDOUT :
				 mode == 2		? ECANCELED :
							  0
		};
		int64_t blocker = -1;
		if (mode == 4) {
			blocker = machine_coroutine_create(tls_worker_blocker,
							   &queue);
			test(blocker != -1);
			while (!atomic_load(&queue.blocked)) {
				machine_sleep(1);
			}
		}
		int64_t server =
			machine_coroutine_create(tls_worker_server, &t);
		test(server != -1);
		if (mode == 0) {
			int64_t client =
				machine_coroutine_create(tls_worker_client, &t);
			test(client != -1);
			test(machine_join(client) == 0);
		} else if (mode == 2) {
			machine_sleep(10);
			test(machine_cancel(server) == 0);
		} else if (mode == 3) {
			test(mm_io_close(t.client) == 0);
		} else if (mode == 4) {
			machine_sleep(40);
			atomic_store(&queue.release, 1);
			test(machine_join(blocker) == 0);
		}
		if (!t.done) {
			test(machine_join(server) == 0);
		}
		test(t.done);
		if (mode != 0) {
			if (mode != 3) {
				test(mm_io_close(t.client) == 0);
			}
			mm_io_free(t.client);
		}
	}

	/* One TLS machine must interleave many handshakes, including a silent
	 * client, without creating a coroutine for each handshake on that machine. */
	tls_worker_test_t concurrent[33] = { 0 };
	int64_t servers[33], clients[32];
	for (int i = 0; i < 33; i++) {
		int sockets[2];
		test(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
		concurrent[i] = (tls_worker_test_t){
			.server = tls_worker_io(sockets[0], 1),
			.client = tls_worker_io(sockets[1], 0),
			.tls = tls,
			.timeout = i == 32 ? 200 : 5000,
			.success = i != 32,
			.error = i == 32 ? ETIMEDOUT : 0
		};
		servers[i] = machine_coroutine_create(tls_worker_server,
						      &concurrent[i]);
		test(servers[i] != -1);
		if (i < 32) {
			clients[i] = machine_coroutine_create(tls_worker_client,
							      &concurrent[i]);
			test(clients[i] != -1);
		}
	}
	for (int i = 0; i < 33; i++) {
		if (!concurrent[i].done) {
			test(machine_join(servers[i]) == 0);
		}
		if (i < 32) {
			int rc = machine_join(clients[i]);
			test(rc == 0 || machine_errno() == ENOENT);
		}
	}
	test(mm_io_close(concurrent[32].client) == 0);
	mm_io_free(concurrent[32].client);
	machine_tls_free(tls);
	od_tls_workers_destroy();
	test(!od_tls_workers_enabled());
}

void odyssey_test_tls_workers(void)
{
	test(machinarium_init() == 0);
	int64_t source = machine_create("tls test", tls_worker_tests, NULL);
	test(source != -1);
	test(machine_wait(source) == 0);
	machinarium_free();
}
