
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
	mm_t env = arg;
	printf("worker started.\n");
	while (mm_is_online(env)) {
		csw++;
		mm_sleep(env, 0);
	}
	printf("worker done.\n");
}

static void
benchmark_runner(void *arg)
{
	mm_t env = arg;
	printf("benchmark started.\n");
	mm_create(env, benchmark_worker, env);
	mm_sleep(env, 1000);
	printf("done.\n");
	printf("context switches %d in 1 sec.\n", csw);
	mm_stop(env);
}

int
main(int argc, char *argv[])
{
	mm_t env = mm_new();
	mm_create(env, benchmark_runner, env);
	mm_start(env);
	mm_free(env);
	return 0;
}
