
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
machinarium_test_read_10mb1(void)
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
