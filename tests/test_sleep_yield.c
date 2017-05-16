
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

static void
test_sleep_yield_fiber(void *arg)
{
	machine_t machine = arg;
	machine_sleep(machine, 0);
	machine_stop(machine);
}

void
test_sleep_yield(void)
{
	machine_t machine = machine_create();
	test(machine != NULL);

	int rc;
	rc = machine_create_fiber(machine, test_sleep_yield_fiber, machine);
	test(rc != -1);

	machine_start(machine);

	rc = machine_free(machine);
	test(rc != -1);
}
