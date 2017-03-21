
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <assert.h>

static void
test_condition(void *arg)
{
	machine_t machine = arg;
	printf("condition fiber started\n");
	int rc = machine_condition(machine, 1);
	assert(rc < 0);
	printf("condition fiber ended\n");
}

static void
test_waiter(void *arg)
{
	machine_t machine = arg;

	printf("waiter started\n");

	int64_t a;
	a = machine_create_fiber(machine, test_condition, machine);
	machine_sleep(machine, 100);

	int rc;
	rc = machine_signal(machine, a);
	assert(rc < 0);

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
