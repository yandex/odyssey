
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

#include <arpa/inet.h>

static void
test_server(void *arg)
{
	machine_io_t server = machine_create_io();
	test(server != NULL);

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(7778);
	int rc;
	rc = machine_bind(server, (struct sockaddr*)&sa);
	test(rc == 0);

	machine_io_t client;
	rc = machine_accept(server, &client, 16, 100);
	test(rc == -1);
	test(machine_cancelled());
	test(! machine_accept_timedout(server));

	rc = machine_close(server);
	test(rc == 0);

	machine_free_io(server);
}

static void
test_waiter(void *arg)
{
	int id = machine_create_fiber(test_server, NULL);
	test(id != -1);

	machine_sleep(0);

	int rc;
	rc = machine_cancel(id);
	test(rc == 0);

	rc = machine_wait(id);
	test(rc == 0);
}

void
test_accept_cancel(void)
{
	machinarium_init();

	int id;
	id = machine_create("test", test_waiter, NULL);
	test(id != -1);

	int rc;
	rc = machine_join(id);
	test(rc != -1);

	machinarium_free();
}
