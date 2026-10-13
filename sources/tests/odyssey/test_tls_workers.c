#include <fcntl.h>
#include <sys/socket.h>
#include <machinarium/machine.h>
#include <machinarium/io.h>
#include <tests/odyssey_test.h>
#include <frontend.h>
#include <global.h>
#include <instance.h>
#include <router.h>
#include <system.h>
#include <cron.h>
#include <extension.h>
#include <worker_pool.h>
#include <msg.h>
#include <tls_workers.h>

typedef struct {
	od_instance_t instance;
	od_global_t global;
	od_router_t router;
	od_system_server_t source;
	od_config_listen_t listen;
	od_tls_opts_t opts;
	od_cron_t cron;
	od_extension_t extensions;
	od_worker_pool_t pool;
	od_worker_t worker;
	machine_tls_t *tls;
} tls_test_t;

static mm_io_t *test_io(int fd, int accepted)
{
	test(fcntl(fd, F_SETFL, O_NONBLOCK) == 0);
	mm_io_t *io = mm_io_create();
	test(io != NULL);
	io->is_unix_socket = 1;
	io->accepted = accepted;
	io->connected = 1;
	test(mm_io_socket_set(io, fd) == 0);
	if (!accepted) {
		test(mm_io_attach(io) == 0);
	}
	return io;
}

static mm_io_t *submit(tls_test_t *t, bool expired)
{
	int sockets[2];
	test(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
	od_client_t *client = od_client_allocate();
	test(client != NULL);
	client->io.io = test_io(sockets[0], 1);
	client->global = &t->global;
	client->source = &t->source;
	client->tls = t->tls;
	client->time_accept = machine_time_us() - (expired ? 10000000 : 0);
	od_id_generate(&client->id, "c");
	atomic_fetch_add(&t->router.clients, 1);
	atomic_fetch_add(&t->source.clients, 1);
	test(od_routing_slot_acquire(&t->global, 1000) == 0);
	machine_msg_t *msg = machine_msg_create(sizeof(client));
	test(msg != NULL);
	*(od_client_t **)machine_msg_data(msg) = client;
	machine_msg_set_type(msg, OD_MSG_CLIENT_NEW);
	mm_io_t *peer = test_io(sockets[1], 0);
	od_tls_workers_feed(msg);
	return peer;
}

static void request(mm_io_t *peer, uint32_t code, char response)
{
	uint32_t words[] = { htonl(8), htonl(code) };
	machine_msg_t *msg = machine_msg_create(0);
	test(msg != NULL);
	test(machine_msg_write(msg, words, sizeof(words)) == 0);
	test(machine_write(peer, msg, 1000) == 0);
	msg = machine_read(peer, 1, 1000);
	test(msg != NULL);
	test(*(char *)machine_msg_data(msg) == response);
	machine_msg_free(msg);
}

static void startup(mm_io_t *peer, bool pipeline)
{
	kiwi_fe_arg_t args[] = {
		{ "user", 5 }, { "u", 2 }, { "database", 9 }, { "d", 2 }
	};
	machine_msg_t *msg = kiwi_fe_write_startup_message(NULL, 4, args);
	test(msg != NULL);
	if (pipeline) {
		test(machine_msg_write(msg, "hello", 5) == 0);
	}
	test(machine_write(peer, msg, 1000) == 0);
}

static od_client_t *receive(tls_test_t *t)
{
	machine_msg_t *msg = machine_channel_read(t->worker.task_channel, 2000);
	test(msg != NULL);
	test(machine_msg_type(msg) == OD_MSG_CLIENT_NEW);
	od_client_t *client = *(od_client_t **)machine_msg_data(msg);
	machine_msg_free(msg);
	test(!client->io.io->attached);
	return client;
}

static void close_client(tls_test_t *t, od_client_t *client)
{
	od_frontend_close(client);
	od_routing_slot_release(&t->global);
}

static void close_peer(mm_io_t *peer)
{
	test(mm_io_close(peer) == 0);
	mm_io_free(peer);
}

static void wait_closed(tls_test_t *t)
{
	for (int i = 0; i < 2000 && (atomic_load(&t->router.clients) ||
				     mm_sem_in_use(&t->global.routing_sem));
	     i++) {
		machine_sleep(1);
	}
	test(atomic_load(&t->router.clients) == 0);
	test(atomic_load(&t->source.clients) == 0);
	test(mm_sem_in_use(&t->global.routing_sem) == 0);
	test(machine_channel_read(t->worker.task_channel, 0) == NULL);
}

static void tls_tests(void *arg)
{
	tls_test_t *t = arg;
	t->worker.task_channel = machine_channel_create();
	test(t->worker.task_channel != NULL);
	t->tls = machine_tls_create();
	test(t->tls != NULL);
	test(machine_tls_set_verify(t->tls, "none") == 0);
	test(machine_tls_set_ca_file(t->tls, "./machinarium/ca.crt") == 0);
	test(machine_tls_set_cert_file(t->tls, "./machinarium/server.crt") ==
	     0);
	test(machine_tls_set_key_file(t->tls, "./machinarium/server.key") == 0);
	test(od_tls_workers_init(1) == 0);

	/* A client waiting for a packet must not hold up unrelated handshakes. */
	mm_io_t *silent = submit(t, false);
	for (int mode = 0; mode < 4; mode++) {
		mm_io_t *peer = submit(t, false);
		if (mode >= 2) {
			request(peer, NEGOTIATE_GSS_CODE, 'N');
		}
		bool ssl = mode % 2;
		if (ssl) {
			request(peer, NEGOTIATE_SSL_CODE, 'S');
			test(mm_io_set_tls(peer, t->tls, 1000) == 0);
			/* The TLS stage transfers before the encrypted StartupMessage. */
		} else {
			startup(peer, true);
		}
		od_client_t *client = receive(t);
		test(client->startup_ssl_done == ssl);
		test(client->startup_gss_done == (mode >= 2));
		test(client->startup_received == !ssl);
		test(mm_io_attach(client->io.io) == 0);
		char text[5];
		if (ssl) {
			test(client->io.readahead.buf == NULL);
			machine_msg_t *msg = machine_msg_create(0);
			test(msg != NULL);
			test(machine_msg_write(msg, "hello", 5) == 0);
			test(machine_write(peer, msg, 1000) == 0);
			msg = machine_read(client->io.io, 5, 1000);
			test(msg != NULL);
			memcpy(text, machine_msg_data(msg), 5);
			machine_msg_free(msg);
		} else {
			test(client->io.readahead.buf != NULL);
			test(od_io_read(&client->io, text, 5, 1000) == 0);
		}
		test(memcmp(text, "hello", 5) == 0);
		close_client(t, client);
		close_peer(peer);
	}
	test(atomic_load(&t->router.clients) == 1);
	close_peer(silent);
	wait_closed(t);

	/* Preserve negotiation flags across the boundary: a second SSLRequest
	 * inside TLS must be rejected by the regular frontend. */
	mm_io_t *peer = submit(t, false);
	request(peer, NEGOTIATE_SSL_CODE, 'S');
	test(mm_io_set_tls(peer, t->tls, 1000) == 0);
	od_client_t *client = receive(t);
	uint32_t again[] = { htonl(8), htonl(NEGOTIATE_SSL_CODE) };
	machine_msg_t *msg = machine_msg_create(0);
	test(msg != NULL);
	test(machine_msg_write(msg, again, sizeof(again)) == 0);
	test(machine_write(peer, msg, 1000) == 0);
	od_frontend(client);
	close_peer(peer);
	wait_closed(t);

	/* Queue expiry, EOF during a handshake, and a silent handshake timeout
	 * release the client counters and routing slot without forwarding it. */
	peer = submit(t, true);
	wait_closed(t);
	close_peer(peer);
	peer = submit(t, false);
	request(peer, NEGOTIATE_SSL_CODE, 'S');
	close_peer(peer);
	wait_closed(t);
	t->listen.client_login_timeout = 30;
	peer = submit(t, false);
	request(peer, NEGOTIATE_SSL_CODE, 'S');
	wait_closed(t);
	close_peer(peer);
	t->listen.client_login_timeout = 5000;

	/* Shutdown drains both queued messages and active negotiations. */
	mm_io_t *peers[32];
	for (int i = 0; i < 32; i++) {
		peers[i] = submit(t, false);
		startup(peers[i], false);
	}
	od_tls_workers_destroy();
	test(!od_tls_workers_enabled());
	for (int i = 0; i < 32; i++) {
		client = receive(t);
		test(client->startup_received);
		test(client->io.readahead.buf == NULL);
		close_client(t, client);
		close_peer(peers[i]);
	}
	wait_closed(t);
	machine_tls_free(t->tls);
	machine_channel_free(t->worker.task_channel);
}

void odyssey_test_tls_workers(void)
{
	test(machinarium_init() == 0);
	tls_test_t *t = calloc(1, sizeof(*t));
	test(t != NULL);
	od_pid_init(&t->instance.pid);
	test(od_logger_init(&t->instance.logger, &t->instance.pid) == 0);
	t->instance.logger.log_stdout = 0;
	t->instance.config.readahead = 4096;
	t->instance.config.cache_coroutine = 8;
	t->global.instance = &t->instance;
	t->global.router = &t->router;
	t->global.cron = &t->cron;
	t->global.worker_pool = &t->pool;
	t->global.extensions = &t->extensions;
	mm_sem_init(&t->global.routing_sem, 128);
	t->pool.pool = &t->worker;
	t->pool.count = 1;
	t->source.config = &t->listen;
	t->listen.tls_opts = &t->opts;
	t->opts.tls_mode = OD_CONFIG_TLS_ALLOW;
	t->listen.client_login_timeout = 5000;
	od_global_set(&t->global);
	int64_t source = machine_create("handoff test", tls_tests, t);
	test(source != -1);
	test(machine_wait(source) == 0);
	od_global_set(NULL);
	mm_sem_destroy(&t->global.routing_sem);
	od_logger_close(&t->instance.logger);
	free(t);
	machinarium_free();
}
