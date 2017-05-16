
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

static int csw = 0;

static void
csw_worker(void *arg)
{
	machine_t machine = arg;
	while (csw < 100000) {
		machine_sleep(machine, 0);
		csw++;
	}
}

static void
csw_runner(void *arg)
{
	machine_t machine = arg;

	int rc;
	rc = machine_create_fiber(machine, csw_worker, machine);
	test(rc != -1);

	rc = machine_wait(machine, rc);
	test(rc != -1);
	test(csw == 100000);

	machine_stop(machine);
}

void
test_context_switch(void)
{
	machine_t machine = machine_create();
	test(machine != NULL);

	int rc;
	rc = machine_create_fiber(machine, csw_runner, machine);
	test(rc != -1);

	machine_start(machine);

	rc = machine_free(machine);
	test(rc != -1);
}
