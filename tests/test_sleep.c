
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <assert.h>

static void
test_child(void *arg)
{
	machine_t machine = arg;
	printf("child started\n");
	printf("sleep 10 ms\n");
	machine_sleep(machine, 1000);
	printf("sleep wakeup\n");
	printf("child ended\n");
	machine_stop(machine);
}

int
main(int argc, char *argv[])
{
	machine_t machine = machine_create();
	machine_create_fiber(machine, test_child, machine);
	machine_start(machine);
	machine_free(machine);
	return 0;
}
