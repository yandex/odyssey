
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <assert.h>

static void
fiber(void *arg)
{
	machine_t machine = arg;
	machine_io_t io = machine_create_io(machine);
	struct addrinfo *res = NULL;
	int rc = machine_getaddrinfo(io, "abracadabra", "http", NULL, &res, 0);
	assert(rc < 0);
	assert(res == NULL);
	machine_close(io);
	machine_stop(machine);
}

int
main(int argc, char *argv[])
{
	machine_t machine = machine_create();
	machine_create_fiber(machine, fiber, machine);
	machine_start(machine);
	machine_free(machine);
	return 0;
}
