
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
	machine_sleep(100);
}

static void
test_child_b(void *arg)
{
	machine_sleep(300);
}

static void
test_waiter(void *arg)
{
	int64_t a, b;
	b = machine_create_fiber(test_child_b, NULL);
	test(b != -1);

	a = machine_create_fiber(test_child_a, NULL);
	test(a != -1);

	int rc;
	rc = machine_wait(a);
	test(rc == 0);

	rc = machine_wait(b);
	test(rc == 0);

	machine_stop();
}

void
test_wait(void)
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
