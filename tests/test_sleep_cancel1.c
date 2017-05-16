
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

static void
test_sleep_cancel1_fiber(void *arg)
{
	machine_t machine = arg;
	test(machine_cancelled(machine))
	machine_sleep(machine, 6000000);
	machine_stop(machine);
}

void
test_sleep_cancel1(void)
{
	machine_t machine = machine_create();
	test(machine != NULL);

	int rc;
	rc = machine_create_fiber(machine, test_sleep_cancel1_fiber, machine);
	test(rc != -1);

	rc = machine_cancel(machine, rc);
	test(rc == 0);

	machine_start(machine);

	rc = machine_free(machine);
	test(rc != -1);
}
