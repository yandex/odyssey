
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

/*
 * This example shows fiber context switch (yield)
 * performance done in one second.
*/

#include <machinarium.h>

static int csw = 0;

static void
benchmark_worker(void *arg)
{
	machine_t machine = arg;
	printf("worker started.\n");
	while (machine_active(machine)) {
		csw++;
		machine_sleep(machine, 0);
	}
	printf("worker done.\n");
}

static void
benchmark_runner(void *arg)
{
	machine_t machine = arg;
	printf("benchmark started.\n");
	machine_create_fiber(machine, benchmark_worker, machine);
	machine_sleep(machine, 1000);
	printf("done.\n");
	printf("context switches %d in 1 sec.\n", csw);
	machine_stop(machine);
}

int
main(int argc, char *argv[])
{
	machine_t machine = machine_create();
	machine_create_fiber(machine, benchmark_runner, machine);
	machine_start(machine);
	machine_free(machine);
	return 0;
}
