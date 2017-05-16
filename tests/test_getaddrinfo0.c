
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

static void
test_gai(void *arg)
{
	machine_t machine = arg;
	machine_io_t io = machine_create_io(machine);
	test(io != NULL);
	struct addrinfo *res = NULL;
	int rc = machine_getaddrinfo(io, "localhost", "http", NULL, &res, INT_MAX);
	if (rc < 0) {
		printf("failed to resolve address\n");
	} else {
		test(res != NULL);
		if (res)
			freeaddrinfo(res);
	}
	machine_free_io(io);
	machine_stop(machine);
}

void
test_getaddrinfo0(void)
{
	machine_t machine = machine_create();
	test(machine != NULL);

	int rc;
	rc = machine_create_fiber(machine, test_gai, machine);
	test(rc != -1);

	machine_start(machine);

	rc = machine_free(machine);
	test(rc != -1);
}
