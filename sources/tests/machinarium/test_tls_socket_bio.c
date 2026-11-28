
#include <machinarium/bio.h>
#include <machinarium/machine.h>
#include <machinarium/socket.h>
#include <tests/odyssey_test.h>

struct transport {
	size_t bytes_read;
	size_t bytes_written;
	int short_write;
	int retry_write;
	int retry_read;
};

static ssize_t transport_retry(void)
{
	/* The transport reports its error through machine_errno(). */
	errno = EIO;
	mm_errno_set(EAGAIN);
	return -1;
}

static ssize_t transport_read(mm_bio_t *bio, void *buf, size_t size)
{
	struct transport *transport = bio->state;
	if (transport->retry_read) {
		transport->retry_read = 0;
		return transport_retry();
	}
	ssize_t rc = mm_bio_read(bio->next, buf, size);
	if (rc > 0) {
		transport->bytes_read += rc;
	}
	return rc;
}

static ssize_t transport_write(mm_bio_t *bio, const void *buf, size_t size,
			       size_t *processed)
{
	struct transport *transport = bio->state;
	if (transport->short_write) {
		test(size > 7);
		size = 7;
		transport->short_write = 0;
		transport->retry_write = 1;
	} else if (transport->retry_write) {
		transport->retry_write = 0;
		return transport_retry();
	}
	ssize_t rc = mm_bio_write(bio->next, buf, size, processed);
	if (rc > 0) {
		transport->bytes_written += rc;
	}
	return rc;
}

static mm_io_t *open_peer(int fd)
{
	mm_io_t *io = mm_io_create();
	test(io != NULL);
	io->is_unix_socket = 1;
	test(mm_io_socket_set(io, fd) == 0);
	test(mm_socket_set_nonblock(fd, 1) == 0);
	test(mm_io_attach(io) == 0);
	io->connected = 1;
	return io;
}

struct server_handshake {
	mm_io_t *io;
	machine_tls_t *tls;
	int done;
};

static void server_handshake(void *arg)
{
	struct server_handshake *server = arg;
	test(mm_io_set_tls(server->io, server->tls, 5000) == 0);
	server->done = 1;
}

static void test_exchange(void *arg)
{
	(void)arg;
	int fds[2];
	test(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	mm_io_t *server = open_peer(fds[0]);
	mm_io_t *client = open_peer(fds[1]);
	server->accepted = 1;

	struct transport transport = { 0 };
	mm_bio_methods_t methods = mm_bio_socket_methods;
	methods.read = transport_read;
	methods.write = transport_write;
	mm_bio_t probe;
	mm_bio_init(&probe, &methods, client, &transport);
	mm_bio_push(&client->bio, &probe);

	machine_tls_t *server_tls = machine_tls_create();
	test(server_tls != NULL);
	test(machine_tls_set_cert_file(server_tls,
				       "./machinarium/server.crt") == 0);
	test(machine_tls_set_key_file(server_tls, "./machinarium/server.key") ==
	     0);
	struct server_handshake handshake = { .io = server, .tls = server_tls };
	int id = machine_coroutine_create(server_handshake, &handshake);
	test(id != -1);
	machine_tls_t *tls = machine_tls_create();
	test(tls != NULL);
	test(machine_tls_set_ca_file(tls, "./machinarium/ca.crt") == 0);
	test(mm_io_set_tls(client, tls, 5000) == 0);
	if (!handshake.done) {
		test(machine_join(id) == 0);
	}
	test(transport.bytes_read > 0);
	test(transport.bytes_written > 0);

	char payload[1024];
	for (size_t i = 0; i < sizeof(payload); i++) {
		payload[i] = (char)(i * 31);
	}
	transport.bytes_written = 0;
	transport.short_write = 1;
	test(mm_io_write(client, payload, sizeof(payload), NULL) == -1);
	test(machine_errno() == EAGAIN);
	test(mm_io_want_write(client));
	test(transport.bytes_written == 7);
	test(mm_io_write(client, payload, sizeof(payload), NULL) ==
	     sizeof(payload));
	test(transport.bytes_written > sizeof(payload));

	machine_msg_t *msg = mm_io_read_msg(server, sizeof(payload), 5000);
	test(msg != NULL);
	test(memcmp(machine_msg_data(msg), payload, sizeof(payload)) == 0);
	char received[sizeof(payload)];
	test(mm_io_read(server, received, 1) == -1);
	test(machine_errno() == EAGAIN);
	test(mm_io_write_msg(server, msg, 5000) == 0);

	transport.retry_read = 1;
	transport.bytes_read = 0;
	test(mm_io_read(client, received, sizeof(received)) == -1);
	test(machine_errno() == EAGAIN);
	test(mm_io_want_read(client));
	msg = mm_io_read_msg(client, sizeof(payload), 5000);
	test(msg != NULL);
	test(memcmp(machine_msg_data(msg), payload, sizeof(payload)) == 0);
	machine_msg_free(msg);
	test(transport.bytes_read > sizeof(payload));
	test(mm_io_read(client, received, 1) == -1);
	test(machine_errno() == EAGAIN);

	/* An empty socket, a cached transport error, and peer EOF in turn. */
	BIO *rbio = SSL_get_rbio(client->tls_ssl);
	test(BIO_read(rbio, received, 0) == 0);
	test(!BIO_eof(rbio));
	test(client->connected);
	test(BIO_read(rbio, received, 1) == -1);
	test(BIO_should_read(rbio));
	test(!BIO_eof(rbio));
	client->error = ECONNRESET;
	test(BIO_read(rbio, received, 1) == -1);
	test(!BIO_should_retry(rbio));
	test(errno == ECONNRESET);
	test(!BIO_eof(rbio));
	client->error = 0;
	test(mm_io_close(server) == 0);
	test(BIO_read(rbio, received, 1) == 0);
	test(!BIO_should_retry(rbio));
	test(BIO_eof(rbio));

	test(mm_io_close(client) == 0);
	mm_io_free(server);
	mm_io_free(client);
	machine_tls_free(tls);
	machine_tls_free(server_tls);
}

void machinarium_test_tls_socket_bio(void)
{
	test(machinarium_init() == 0);
	int id = machine_create("test", test_exchange, NULL);
	test(id != -1);
	test(machine_wait(id) == 0);
	machinarium_free();
}
