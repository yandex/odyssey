
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

#include <string.h>
#include <arpa/inet.h>

static void
server(void *arg)
{
	machine_io_t server = machine_io_create();
	test(server != NULL);

	int rc;
	rc = machine_set_readahead(server, 16384);
	test(rc == 0);

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(7778);
	rc = machine_bind(server, (struct sockaddr*)&sa);
	test(rc == 0);

	machine_io_t client;
	rc = machine_accept(server, &client, 16, UINT32_MAX);
	test(rc == 0);

	machine_tls_t *tls;
	tls = machine_tls_create();
	rc = machine_tls_set_verify(tls, "none");
	test(rc == 0);
	rc = machine_tls_set_ca_file(tls, "./ca.crt");
	test(rc == 0);
	rc = machine_tls_set_cert_file(tls, "./server.crt");
	test(rc == 0);
	rc = machine_tls_set_key_file(tls, "./server.key");
	test(rc == 0);
	rc = machine_set_tls(client, tls);
	if (rc == -1) {
		printf("%s\n", machine_error(client));
		test(rc == 0);
	}

	int chunk_size = 10 * 1024;
	char *chunk = malloc(chunk_size);
	test(chunk != NULL);
	memset(chunk, 'x', chunk_size);

	int total = 10 * 1024 * 1024;
	int pos = 0;
	while (pos < total) {
		rc = machine_write(client, chunk, chunk_size, UINT32_MAX);
		test(rc == 0);
		pos += chunk_size;
	}
	free(chunk);

	rc = machine_close(client);
	test(rc == 0);
	machine_io_free(client);

	rc = machine_close(server);
	test(rc == 0);
	machine_io_free(server);

	machine_tls_free(tls);
}

static void
client(void *arg)
{
	machine_io_t client = machine_io_create();
	test(client != NULL);

	int rc;
	rc = machine_set_readahead(client, 16384);
	test(rc == 0);

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(7778);
	rc = machine_connect(client, (struct sockaddr*)&sa, UINT32_MAX);
	test(rc == 0);

	machine_tls_t *tls;
	tls = machine_tls_create();
	rc = machine_tls_set_verify(tls, "none");
	test(rc == 0);
	rc = machine_tls_set_ca_file(tls, "./ca.crt");
	test(rc == 0);
	rc = machine_tls_set_cert_file(tls, "./client.crt");
	test(rc == 0);
	rc = machine_tls_set_key_file(tls, "./client.key");
	test(rc == 0);
	rc = machine_set_tls(client, tls);
	if (rc == -1) {
		printf("%s\n", machine_error(client));
		test(rc == 0);
	}

	char buf[1024];
	int pos = 0;
	while (1) {
		rc = machine_read(client, buf, 1024, UINT32_MAX);
		if (rc == -1)
			break;
		pos += 1024;
	}
	test(pos == 10 * 1024 * 1024);

	rc = machine_close(client);
	test(rc == 0);
	machine_io_free(client);

	machine_tls_free(tls);
}

static void
test_cs(void *arg)
{
	int rc;
	rc = machine_coroutine_create(server, NULL);
	test(rc != -1);

	rc = machine_coroutine_create(client, NULL);
	test(rc != -1);
}

void
test_tls_read_10mb(void)
{
	machinarium_init();

	int id;
	id = machine_create("test", test_cs, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}
