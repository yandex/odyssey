
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

static void
test_condition_fiber(void *arg)
{
	machine_t machine = arg;
	int rc = machine_condition(machine, 1);
	test(rc == -1);
}

static void
test_waiter(void *arg)
{
	machine_t machine = arg;

	int64_t a;
	a = machine_create_fiber(machine, test_condition_fiber, machine);
	test(a != -1);

	machine_sleep(machine, 100);

	int rc;
	rc = machine_signal(machine, a);
	test(rc == -1);

	machine_stop(machine);
}

void
test_condition1(void)
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
