
#include <machinarium/machinarium.h>
#include <machinarium/io.h>
#include <machinarium/socket.h>
#include <machinarium/tls.h>
#include <tests/odyssey_test.h>

static int verify_result(X509_STORE_CTX *ctx, void *arg)
{
	X509_STORE_CTX_set_error(ctx, *(int *)arg);
	/* Complete the handshake so Machinarium checks the verification result. */
	return 1;
}

static void server_handshake(void *arg)
{
	mm_io_t *io = arg;
	test(mm_tls_handshake(io, 5000) == 0);
}

static void test_verify_error(void *arg)
{
	(void)arg;
	machine_tls_t *server_tls = machine_tls_create();
	machine_tls_t *client_tls = machine_tls_create();
	test(server_tls != NULL);
	test(client_tls != NULL);
	test(machine_tls_set_cert_file(server_tls,
				       "./machinarium/server.crt") == 0);
	test(machine_tls_set_key_file(server_tls, "./machinarium/server.key") ==
	     0);

	int errors[] = { X509_V_OK, X509_V_ERR_PERMITTED_VIOLATION,
			 X509_V_ERR_CERT_HAS_EXPIRED, X509_V_ERR_INVALID_CA };
	for (size_t i = 0; i < sizeof(errors) / sizeof(errors[0]); i++) {
		int fds[2];
		test(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
		mm_io_t *server = mm_io_create();
		mm_io_t *client = mm_io_create();
		test(server != NULL);
		test(client != NULL);

		server->is_unix_socket = 1;
		client->is_unix_socket = 1;
		test(mm_io_socket_set(server, fds[0]) == 0);
		test(mm_io_socket_set(client, fds[1]) == 0);
		test(mm_socket_set_nonblock(fds[0], 1) == 0);
		test(mm_socket_set_nonblock(fds[1], 1) == 0);
		test(mm_io_attach(server) == 0);
		test(mm_io_attach(client) == 0);
		server->accepted = 1;
		server->connected = 1;
		client->connected = 1;
		server->tls = (mm_tls_t *)server_tls;
		client->tls = (mm_tls_t *)client_tls;

		SSL_CTX *ctx = mm_tls_get_context(client, 1);
		test(ctx != NULL);
		SSL_CTX_set_cert_verify_callback(ctx, verify_result,
						 &errors[i]);

		int id = machine_coroutine_create(server_handshake, server);
		test(id != -1);
		int rc = mm_tls_handshake(client, 5000);
		test(SSL_get_verify_result(client->tls_ssl) == errors[i]);
		if (errors[i] == X509_V_OK) {
			test(rc == 0);
			test(client->tls_error == 0);
		} else {
			test(rc == -1);
			test(client->tls_error == 1);
			const char *error = mm_io_error(client);
			test(strstr(error, X509_verify_cert_error_string(
						   errors[i])) != NULL);
			char code[64];
			snprintf(code, sizeof(code),
				 "SSL_get_verify_result(): %d", errors[i]);
			test(strstr(error, code) != NULL);
		}
		test(machine_join(id) == 0);
		test(mm_io_close(client) == 0);
		test(mm_io_close(server) == 0);
		mm_io_free(client);
		mm_io_free(server);
	}
	machine_tls_free(client_tls);
	machine_tls_free(server_tls);
}

void machinarium_test_tls_verify_error(void)
{
	test(machinarium_init() == 0);
	int id = machine_create("test", test_verify_error, NULL);
	test(id != -1);
	test(machine_wait(id) == 0);
	machinarium_free();
}
