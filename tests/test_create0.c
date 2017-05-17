
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

static int fiber_call = 0;

static void
fiber(void *arg)
{
	fiber_call++;
	machine_stop();
}

void
test_create0(void)
{
	machinarium_init();

	int id;
	id = machine_create("test", fiber, NULL);
	test(id != -1);

	int rc;
	rc = machine_join(id);
	test(rc != -1);

	test(fiber_call == 1);

	machinarium_free();
}
