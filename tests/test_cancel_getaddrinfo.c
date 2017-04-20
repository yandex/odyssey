
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <assert.h>

static void
test_gai(void *arg)
{
	printf("child started\n");
	machine_t machine = arg;
	machine_io_t io = machine_create_io(machine);
	struct addrinfo *res = NULL;
	int rc = machine_getaddrinfo(io, "abracadabra", "http", NULL, &res, INT_MAX);
	assert(rc < 0);
	machine_close(io);
	assert(res == NULL);
	if (machine_cancelled(machine))
		printf("child marked as cancel\n");
	printf("child done\n");
}

static void
test_waiter(void *arg)
{
	machine_t machine = arg;

	printf("waiter started\n");

	int64_t fiber;
	fiber = machine_create_fiber(machine, test_gai, machine);
	machine_sleep(machine, 0);
	machine_cancel(machine, fiber);
	machine_wait(machine, fiber);

	printf("waiter ended \n");
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
