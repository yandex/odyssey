
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
	machine_t machine = arg;

	machine_io_t server = machine_create_io(machine);
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
	test(machine_cancelled(machine));
	test(! machine_accept_timedout(server));

	rc = machine_close(server);
	test(rc == 0);

	machine_free_io(server);
	machine_stop(machine);
}

static void
test_waiter(void *arg)
{
	machine_t machine = arg;

	int id = machine_create_fiber(machine, test_server, machine);
	test(id != -1);

	machine_sleep(machine, 0);

	int rc;
	rc = machine_cancel(machine, id);
	test(rc == 0);

	rc = machine_wait(machine, id);
	test(rc == 0);

	machine_stop(machine);
}

void
test_accept_cancel(void)
{
	machine_t machine = machine_create();
	test(machine != NULL);

	int rc;
	rc = machine_create_fiber(machine, test_waiter, machine);
	test(rc != -1);

	machine_start(machine);

	rc = machine_free(machine);
	test(rc != -1);
}
