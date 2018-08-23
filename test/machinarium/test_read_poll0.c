
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

	machine_io_t *io_set_ready[] = {NULL};
	machine_io_t *io_set[] = {client};

	machine_msg_t *msg;
	msg = machine_read(client, 1, UINT32_MAX);
	test(msg != NULL);
	machine_msg_free(msg);

	rc = machine_read_poll(io_set, io_set_ready, 1, UINT32_MAX);
	test(rc == 1);
	rc = machine_read_poll(io_set, io_set_ready, 1, UINT32_MAX);
	test(rc == 1);

	msg = machine_read(io_set_ready[0], 1023, UINT32_MAX);
	test(msg != NULL);
	machine_msg_free(msg);

	/* test eof */
	rc = machine_read_poll(io_set, io_set_ready, 1, UINT32_MAX);
	test(rc == 1);
	rc = machine_read_poll(io_set, io_set_ready, 1, UINT32_MAX);
	test(rc == 1);

	msg = machine_read(io_set_ready[0], 1024, UINT32_MAX);
	test(msg == NULL);

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

	machine_msg_t *msg;
	msg = machine_msg_create(0);
	rc = machine_msg_write(msg, NULL, 1024);
	test(rc == 0);
	memset(machine_msg_get_data(msg), 'x', 1024);

	rc = machine_write(client, msg);
	test(rc == 0);

	rc = machine_flush(client, UINT32_MAX);
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
}

void
machinarium_test_read_poll0(void)
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
