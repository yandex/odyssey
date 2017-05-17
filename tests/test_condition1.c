
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
	int rc = machine_condition(1);
	test(rc == -1);
}

static void
test_waiter(void *arg)
{
	int64_t a;
	a = machine_create_fiber(test_condition_fiber, NULL);
	test(a != -1);

	machine_sleep(100);

	int rc;
	rc = machine_signal(a);
	test(rc == -1);

	machine_stop();
}

void
test_condition1(void)
{
	machinarium_init();

	int id;
	id = machine_create("test", test_waiter, NULL);
	test(id != -1);

	int rc;
	rc = machine_join(id);
	test(rc != -1);

	machinarium_free();
}
