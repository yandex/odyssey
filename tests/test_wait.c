
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

static void
test_child_a(void *arg)
{
	machine_t machine = arg;
	machine_sleep(machine, 100);
}

static void
test_child_b(void *arg)
{
	machine_t machine = arg;
	machine_sleep(machine, 300);
}

static void
test_waiter(void *arg)
{
	machine_t machine = arg;

	int64_t a, b;
	b = machine_create_fiber(machine, test_child_b, machine);
	test(b != -1);

	a = machine_create_fiber(machine, test_child_a, machine);
	test(a != -1);

	int rc;
	rc = machine_wait(machine, a);
	test(rc == 0);

	rc = machine_wait(machine, b);
	test(rc == 0);

	machine_stop(machine);
}

void
test_wait(void)
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
