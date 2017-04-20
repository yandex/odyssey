
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>
#include <uv.h>
#include <assert.h>

static void
test_connect(void *arg)
{
	machine_t machine = arg;
	assert(machine_cancelled(machine));
	printf("child started\n");
	machine_io_t client = machine_create_io(machine);

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("8.8.8.8");
	sa.sin_port = htons(1234);

	int rc;
	rc = machine_connect(client, (struct sockaddr*)&sa, INT_MAX);
	printf("child resumed\n");
	assert(rc < 0);
	machine_close(client);
	machine_free_io(client);
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
	machine_cancel(machine, id); /* run cancelled */
	machine_sleep(machine, 0);
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
