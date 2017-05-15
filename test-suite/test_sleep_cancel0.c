
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

static void
test_sleep_cancel0_child(void *arg)
{
	machine_t machine = arg;
	machine_sleep(machine, 6000000);
	test(machine_cancelled(machine))
}

static void
test_sleep_cancel0_parent(void *arg)
{
	machine_t machine = arg;

	int64_t id;
	id = machine_create_fiber(machine, test_sleep_cancel0_child, machine);
	test(id != -1);

	machine_sleep(machine, 0);

	int rc;
	rc = machine_cancel(machine, id);
	test(rc == 0);

	rc = machine_wait(machine, id);
	test(rc == 0);

	machine_stop(machine);
}

void
test_sleep_cancel0(void)
{
	machine_t machine = machine_create();
	test(machine != NULL);

	int rc;
	rc = machine_create_fiber(machine, test_sleep_cancel0_parent, machine);
	test(rc != -1);

	machine_start(machine);

	rc = machine_free(machine);
	test(rc != -1);
}
