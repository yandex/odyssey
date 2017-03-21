
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <assert.h>

int fiber_call = 0;

static void
fiber(void *arg)
{
	machine_t machine = arg;
	fiber_call++;
	machine_stop(machine);
}

int
main(int argc, char *argv[])
{
	machine_t machine = machine_create();
	machine_create_fiber(machine, fiber, machine);
	machine_start(machine);
	assert(fiber_call == 1);
	machine_free(machine);
	return 0;
}
