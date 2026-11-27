
#include <machinarium/io.h>
#include <machinarium/socket.h>
#include <tests/odyssey_test.h>

struct closing_peer {
	mm_io_t *client;
	int fd;
};

static void close_after_client_hello(void *arg)
{
	struct closing_peer *peer = arg;
	test(peer->client->tls_ssl != NULL);
	test(SSL_want_read(peer->client->tls_ssl));

	/* Drain the ClientHello so close delivers EOF on the stream. */
	char buf[1024];
	ssize_t n = recv(peer->fd, buf, sizeof(buf), MSG_DONTWAIT);
	test(n > 0);
	while (n > 0) {
		n = recv(peer->fd, buf, sizeof(buf), MSG_DONTWAIT);
	}
	test(n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
	test(close(peer->fd) == 0);
}

static void test_handshake_peer_close(void *arg)
{
	(void)arg;
	int fds[2];
	test(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	mm_io_t *client = mm_io_create();
	test(client != NULL);
	client->is_unix_socket = 1;
	test(mm_io_socket_set(client, fds[0]) == 0);
	test(mm_socket_set_nonblock(fds[0], 1) == 0);
	test(mm_io_attach(client) == 0);
	client->connected = 1;
	machine_tls_t *tls = machine_tls_create();
	test(tls != NULL);
	struct closing_peer peer = { .client = client, .fd = fds[1] };
	test(machine_coroutine_create(close_after_client_hello, &peer) != -1);
	test(mm_io_set_tls(client, tls, 1000) == -1);
	test(machine_errno() != ETIMEDOUT);
	const char *error = mm_io_error(client);
	test(error != NULL && strstr(error, "SSL_connect()") != NULL);
	test(mm_io_close(client) == 0);
	mm_io_free(client);
	machine_tls_free(tls);
}

void machinarium_test_tls_handshake_peer_close(void)
{
	test(machinarium_init() == 0);
	int id = machine_create("test", test_handshake_peer_close, NULL);
	test(id != -1);
	test(machine_wait(id) == 0);
	machinarium_free();
}
