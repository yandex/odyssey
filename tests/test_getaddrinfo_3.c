
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <assert.h>

static void
fiber_1(void *arg)
{
	machine_t machine = arg;
	machine_io_t io = machine_create_io(machine);
	struct addrinfo *res = NULL;
	int rc = machine_getaddrinfo(io, "localhost", "http", NULL, &res, 0);
	if (rc < 0) {
		printf("failed to resolve address\n");
	} else {
		assert(res != NULL);
	}
	if (res)
		freeaddrinfo(res);
	machine_close(io);
}

static void
fiber_2(void *arg)
{
	machine_t machine = arg;
	machine_io_t io = machine_create_io(machine);
	struct addrinfo *res = NULL;
	int rc = machine_getaddrinfo(io, "localhost", "http", NULL, &res, 0);
	if (rc < 0) {
		printf("failed to resolve address\n");
	} else {
		assert(res != NULL);
	}
	if (res)
		freeaddrinfo(res);
	machine_close(io);
	machine_stop(machine);
}

int
main(int argc, char *argv[])
{
	machine_t machine = machine_create();
	machine_create_fiber(machine, fiber_1, machine);
	machine_create_fiber(machine, fiber_2, machine);
	machine_start(machine);
	machine_free(machine);
	return 0;
}
