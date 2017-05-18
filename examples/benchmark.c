
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
	printf("worker started.\n");
	while (machine_active()) {
		csw++;
		machine_sleep(0);
	}
	printf("worker done.\n");
}

static void
benchmark_runner(void *arg)
{
	printf("benchmark started.\n");
	machine_create_fiber(benchmark_worker, NULL);
	machine_sleep(1000);
	printf("done.\n");
	printf("context switches %d in 1 sec.\n", csw);
	machine_stop();
}

int
main(int argc, char *argv[])
{
	machinarium_init();
	int id = machine_create("benchmark", benchmark_runner, NULL);
	machine_join(id);
	machinarium_free();
	return 0;
}
