
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

	machine_io_t *client_a;
	rc = machine_accept(server, &client_a, 16, 1, UINT32_MAX);
	test(rc == 0);

	machine_io_t *client_b;
	rc = machine_accept(server, &client_b, 16, 1, UINT32_MAX);
	test(rc == 0);

	machine_io_t *client_c;
	rc = machine_accept(server, &client_c, 16, 1, UINT32_MAX);
	test(rc == 0);

	machine_io_t *io_set_ready[3] = {NULL, NULL, NULL};
	machine_io_t *io_set[3] = {client_a, client_b, client_c};
	int io_count = 3;

	while (io_count > 0)
	{
		int ready;
		ready = machine_read_poll(io_set, io_set_ready, io_count, UINT32_MAX);
		test(ready > 0);

		int io_pos = 0;
		while (io_pos < ready) {
			machine_io_t *io = io_set_ready[io_pos];

			char buf[1024];
			rc = machine_read(io, buf, sizeof(buf), UINT32_MAX);
			test(rc == 0);

			/* test eof */
			rc = machine_read(io, buf, sizeof(buf), UINT32_MAX);
			test(rc == -1);

			rc = machine_close(io);
			test(rc == 0);
			machine_io_free(io);

			int i = 0;
			int j = 0;
			while (i < 3) {
				if (io_set[i] == io) {
					/* skip */
					io_set[i] = NULL;
				} else {
					io_set[j] = io_set[i];
					j++;
				}
				i++;
			}
			io_count--;
			io_pos++;
		}
	}

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

	char buf[1024];
	memset(buf, 'x', sizeof(buf));

	rc = machine_write(client, buf, 1024, UINT32_MAX);
	test(rc == 0);

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

	rc = machine_coroutine_create(client, NULL);
	test(rc != -1);

	rc = machine_coroutine_create(client, NULL);
	test(rc != -1);
}

void
machinarium_test_read_poll1(void)
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
