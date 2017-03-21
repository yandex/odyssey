
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <assert.h>

static void
test_child_0(void *arg)
{
	machine_t machine = arg;
	printf("child 0 started\n");
	machine_sleep(machine, 1000);
	printf("child 0 ended\n");
}

static void
test_child_1(void *arg)
{
	machine_t machine = arg;
	printf("child 1 started\n");
	machine_sleep(machine, 500);
	printf("child 1 ended\n");
}

static void
test_waiter(void *arg)
{
	machine_t machine = arg;

	printf("waiter started\n");

	int64_t a, b;
	a = machine_create_fiber(machine, test_child_0, machine);
	b = machine_create_fiber(machine, test_child_1, machine);

	machine_wait(machine, b);
	printf("waiter 1 ended \n");

	machine_wait(machine, a);
	printf("waiter 0 ended \n");

	printf("waiter ended\n");
	machine_stop(machine);
}

int
main(int argc, char *argv[])
{
	machine_t machine = machine_create();
	machine_create_fiber(machine, test_waiter, machine);
	machine_start(machine);
	machine_free(machine);
	return 0;
}
