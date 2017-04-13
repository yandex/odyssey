
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>
#include <assert.h>

static void
test_connect(void *arg)
{
	machine_t machine = arg;
	printf("child started\n");
	machine_io_t server = machine_create_io(machine);

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(7778);

	int rc;
	rc = machine_bind(server, (struct sockaddr*)&sa);
	if (rc < 0) {
		printf("server: bind failed: %s\n", machine_error(server));
		machine_close(server);
		machine_stop(machine);
		return;
	}

	machine_io_t client;
	rc = machine_accept(server, &client, 16, 0);
	if (rc < 0) {
		printf("accept error: %s\n", machine_error(server));
		machine_close(server);
		machine_free_io(server);
		machine_stop(machine);
		return;
	}

	machine_close(client);
	machine_free_io(client);

	machine_close(server);
	machine_free_io(server);

	if (machine_cancelled(machine))
		printf("child marked as cancel\n");
	printf("child end\n");
}

static void
test_waiter(void *arg)
{
	machine_t machine = arg;

	printf("waiter started\n");

	int id = machine_create_fiber(machine, test_connect, machine);
	machine_sleep(machine, 0);
	machine_cancel(machine, id);
	machine_wait(machine, id);

	printf("waiter 1 ended \n");
	machine_stop(machine);
}

int
main(int argc, char *argv[])
{
	machine_t machine = machine_create();
	machine_create_fiber(machine, test_waiter, machine);
	machine_start(machine);
	printf("shutting down\n");
	machine_free(machine);
	return 0;
}
