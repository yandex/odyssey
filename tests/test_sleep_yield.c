
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

static void
fiber(void *arg)
{
	machine_sleep(0);
	machine_stop();
}

void
test_sleep_yield(void)
{
	machinarium_init();

	int id;
	id = machine_create("test", fiber, NULL);
	test(id != -1);

	int rc;
	rc = machine_join(id);
	test(rc != -1);

	machinarium_free();
}
