
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

void
test_io_new(void)
{
	machine_t machine = machine_create();
	test(machine != NULL);

	machine_io_t io = machine_create_io(machine);
	test(io != NULL);

	machine_free_io(io);

	int rc;
	rc = machine_free(machine);
	test(rc != -1);
}
