
#include <machinarium.h>
#include <odyssey_test.h>

#include <string.h>
#include <arpa/inet.h>

static void
server(void *arg)
{
	machine_io_t *server = machine_io_create();
	test(server != NULL);

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(*(int*)arg);
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
	machine_io_t *client = machine_io_create();
	test(client != NULL);

	int rc;
	rc = machine_set_readahead(client, 8096);
	test(rc == 0);

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(*(int*)arg);
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

	machine_io_t *io_set_ready[] = {NULL};
	machine_io_t *io_set[] = {client};
	rc = machine_read_poll(io_set, io_set_ready, 1, UINT32_MAX);
	test(rc == 1);

	char *buf = malloc(10 * 1024 * 1024);
	test(buf != NULL);

	rc = machine_read(client, buf, 10 * 1024 * 1024, UINT32_MAX);
	test(rc == 0);

	char *buf_cmp = malloc(10 * 1024 * 1024);
	test(buf_cmp != NULL);
	memset(buf_cmp, 'x', 10 * 1024 * 1024);
	test(memcmp(buf_cmp, buf, 10 * 1024 * 1024) == 0 );
	free(buf_cmp);

	free(buf);

	rc = machine_close(client);
	test(rc == 0);
	machine_io_free(client);

	machine_tls_free(tls);
}

static void
test_cs(void *arg)
{
	int rc;
	rc = machine_coroutine_create(server, arg);
	test(rc != -1);

	rc = machine_coroutine_create(client, arg);
	test(rc != -1);
}

void
machinarium_test_tls_read_multithread(void)
{
	machinarium_init();

	const int pairs = 10;
	int id[10];
	int port[10];
	int rc;
	int i = 0;
	while (i < pairs) {
		port[i] = i + 7778;
		id[i] = machine_create("test", test_cs, &port[i]);
		test(id[i] != -1);
		i++;
	}
	i = 0;
	while (i < pairs) {
		rc = machine_wait(id[i]);
		test(rc != -1);
		i++;
	}

	machinarium_free();
}
