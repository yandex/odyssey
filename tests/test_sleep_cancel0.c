
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
	machine_sleep(6000000);
	test(machine_cancelled())
}

static void
test_sleep_cancel0_parent(void *arg)
{
	int64_t id;
	id = machine_create_fiber(test_sleep_cancel0_child, NULL);
	test(id != -1);

	machine_sleep(0);

	int rc;
	rc = machine_cancel(id);
	test(rc == 0);

	rc = machine_wait(id);
	test(rc == 0);

	machine_stop();
}

void
test_sleep_cancel0(void)
{
	machinarium_init();

	int id;
	id = machine_create("test", test_sleep_cancel0_parent, NULL);
	test(id != -1);

	int rc;
	rc = machine_join(id);
	test(rc != -1);

	machinarium_free();
}
