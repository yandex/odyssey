
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

static void
test_condition_coroutine(void *arg)
{
	int rc = machine_condition(1000);
	test(rc == 0);
}

static void
test_waiter(void *arg)
{
	int64_t a;
	a = machine_coroutine_create(test_condition_coroutine, NULL);
	test(a != -1);

	machine_sleep(0);

	int rc;
	rc = machine_signal(a);
	test(rc != -1);

	machine_sleep(0);
	machine_stop();
}

void
test_condition0(void)
{
	machinarium_init();

	int id;
	id = machine_create("test", test_waiter, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}
