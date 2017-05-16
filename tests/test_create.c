
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

static int fiber_call;

static void
fiber(void *arg)
{
	machine_t machine = arg;
	fiber_call++;
	machine_stop(machine);
}

void
test_create(void)
{
	fiber_call = 0;
	machine_t machine = machine_create();
	test(machine != NULL);

	int rc;
	rc = machine_create_fiber(machine, fiber, machine);
	test(rc != -1);

	machine_start(machine);
	test(fiber_call == 1);

	rc = machine_free(machine);
	test(rc != -1);
}
