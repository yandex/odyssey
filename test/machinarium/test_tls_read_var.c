
#include <machinarium.h>
#include <odyssey_test.h>

#include <string.h>
#include <arpa/inet.h>

static void
server(void *arg)
{
	(void)arg;
	machine_io_t *server = machine_io_create();
	test(server != NULL);

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(7778);
	int rc;
	rc = machine_bind(server, (struct sockaddr*)&sa);
	test(rc == 0);

	machine_io_t *client;
	rc = machine_accept(server, &client, 16, 1, UINT32_MAX);
	test(rc == 0);

	machine_tls_t *tls;
	tls = machine_tls_create();
	rc = machine_tls_set_verify(tls, "none");
	test(rc == 0);
	rc = machine_tls_set_ca_file(tls, "./machinarium/ca.crt");
	test(rc == 0);
	rc = machine_tls_set_cert_file(tls, "./machinarium/server.crt");
	test(rc == 0);
	rc = machine_tls_set_key_file(tls, "./machinarium/server.key");
	test(rc == 0);
	rc = machine_set_tls(client, tls);
	if (rc == -1) {
		printf("%s\n", machine_error(client));
		test(rc == 0);
	}

	int chunk_size = 100 * 1024;
	char *chunk = malloc(chunk_size);
	test(chunk != NULL);
	memset(chunk, 'x', chunk_size);

	int chunk_pos = 100 * 1024 - 3;
	while (chunk_pos < chunk_size)
	{
		rc = machine_write(client, chunk, chunk_pos, UINT32_MAX);
		test(rc == 0);

		uint32_t ack = 0;
		rc = machine_read(client, (char*)&ack, sizeof(ack), UINT32_MAX);
		test(ack == 1);

		chunk_pos++;
	}

	free(chunk);

	rc = machine_close(client);
	test(rc == 0);
	machine_io_free(client);

	rc = machine_close(server);
	test(rc == 0);
	machine_io_free(server);
}

static void
client(void *arg)
{
	(void)arg;
	machine_io_t *client = machine_io_create();
	test(client != NULL);

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(7778);
	int rc;
	rc = machine_connect(client, (struct sockaddr*)&sa, UINT32_MAX);
	test(rc == 0);

	machine_tls_t *tls;
	tls = machine_tls_create();
	rc = machine_tls_set_verify(tls, "none");
	test(rc == 0);
	rc = machine_tls_set_ca_file(tls, "./machinarium/ca.crt");
	test(rc == 0);
	rc = machine_tls_set_cert_file(tls, "./machinarium/client.crt");
	test(rc == 0);
	rc = machine_tls_set_key_file(tls, "./machinarium/client.key");
	test(rc == 0);
	rc = machine_set_tls(client, tls);
	if (rc == -1) {
		printf("%s\n", machine_error(client));
		test(rc == 0);
	}

	int chunk_size = 100 * 1024;
	char *chunk = malloc(chunk_size);
	test(chunk != NULL);

	char *chunk_cmp = malloc(chunk_size);
	test(chunk_cmp != NULL);
	memset(chunk_cmp, 'x', chunk_size);

	int chunk_pos = 100 * 1024 - 3;
	while (chunk_pos < chunk_size)
	{
		rc = machine_read(client, chunk, chunk_pos, UINT32_MAX);
		test(rc == 0);

		test(memcmp(chunk, chunk_cmp, chunk_pos) == 0);

		uint32_t ack = 1;
		rc = machine_write(client, (char*)&ack, sizeof(ack), UINT32_MAX);
		test(rc == 0);

		chunk_pos++;
	}

	free(chunk);
	free(chunk_cmp);

	rc = machine_close(client);
	test(rc == 0);
	machine_io_free(client);
}

static void
test_cs(void *arg)
{
	(void)arg;
	int rc;
	rc = machine_coroutine_create(server, NULL);
	test(rc != -1);

	rc = machine_coroutine_create(client, NULL);
	test(rc != -1);
}

void
machinarium_test_tls_read_var(void)
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
