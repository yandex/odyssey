
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
	machine_io_t io = machine_create_io();
	test(io != NULL);

	machine_free_io(io);
}

void
test_io_new(void)
{
	machinarium_init();

	int id;
	id = machine_create(fiber, NULL);
	test(id != -1);

	int rc;
	rc = machine_join(id);
	test(rc != -1);

	machinarium_free();
}
