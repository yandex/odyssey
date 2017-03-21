
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
	assert(machine_cancelled(machine));
	printf("child started\n");
	printf("child sleep for 600 seconds\n");
	machine_sleep(machine, 600 * 1000);
	printf("child wakeup\n");
	if (machine_cancelled(machine))
		printf("child cancelled\n");
	printf("child 0 ended\n");
}

static void
test_parent(void *arg)
{
	machine_t machine = arg;

	printf("parent started\n");

	int64_t fiber;
	fiber = machine_create_fiber(machine, test_child, machine);
	machine_cancel(machine, fiber); /* run cancelled fiber */
	machine_sleep(machine, 0);
	machine_wait(machine, fiber);

	printf("parent ended\n");
	machine_stop(machine);
}

int
main(int argc, char *argv[])
{
	machine_t machine = machine_create();
	machine_create_fiber(machine, test_parent, machine);
	machine_start(machine);
	machine_free(machine);
	return 0;
}
