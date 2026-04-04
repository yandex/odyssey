
#include <machinarium/machinarium.h>
#include <machinarium/io.h>
#include <tests/odyssey_test.h>

#include <arpa/inet.h>

static void test_server(void *arg)
{
	(void)arg;
	mm_io_t *server = mm_io_create();
	test(server != NULL);

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(7778);
	int rc;
	rc = mm_io_bind(server, (struct sockaddr *)&sa,
			MM_BINDWITH_SO_REUSEADDR);
	test(rc == 0);

	mm_io_t *client;
	rc = mm_io_accept(server, &client, 16, 1, 100);
	test(rc == -1);
	test(machine_cancelled());
	test(!machine_timedout());

	rc = mm_io_close(server);
	test(rc == 0);

	mm_io_free(server);
}

static void test_waiter(void *arg)
{
	(void)arg;
	int id = machine_coroutine_create(test_server, NULL);
	test(id != -1);

	machine_sleep(0);

	int rc;
	rc = machine_cancel(id);
	test(rc == 0);

	rc = machine_join(id);
	test(rc == 0);
}

void machinarium_test_accept_cancel(void)
{
	machinarium_init();

	int id;
	id = machine_create("test", test_waiter, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}
