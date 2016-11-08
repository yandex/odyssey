
/*
 * flint.
 *
 * Cooperative multitasking engine.
*/

/*
 * This example shows fiber context switch (yield)
 * performance done in one second.
*/

#include <flint.h>

static int csw = 0;

static void
benchmark_worker(void *arg)
{
	ft_t env = arg;
	printf("worker started.\n");
	while (ft_is_online(env)) {
		csw++;
		ft_sleep(env, 0);
	}
	printf("worker done.\n");
}

static void
benchmark_runner(void *arg)
{
	ft_t env = arg;
	printf("benchmark started.\n");
	ft_create(env, benchmark_worker, env);
	ft_sleep(env, 1000);
	printf("done.\n");
	printf("context switches %d in 1 sec.\n", csw);
	ft_stop(env);
}

int
main(int argc, char *argv[])
{
	ft_t env = ft_new();
	ft_create(env, benchmark_runner, env);
	ft_start(env);
	ft_free(env);
	return 0;
}
