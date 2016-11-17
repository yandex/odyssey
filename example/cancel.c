
/*
 * flint.
 *
 * Cooperative multitasking engine.
*/

#include <flint.h>
#include <assert.h>

static void
test_child(void *arg)
{
	ft_t env = arg;
	printf("child started\n");
	ft_sleep(env, 1000000);
	printf("child resumed\n");
	if (ft_is_cancel(env))
		printf("child marked as cancel\n");
	printf("child end\n");
}

static void
test_waiter(void *arg)
{
	ft_t env = arg;

	printf("waiter started\n");

	int id = ft_create(env, test_child, env);

	ft_sleep(env, 0);
	ft_cancel(env, id);
	ft_wait(env, id);

	printf("waiter 1 ended \n");
	ft_stop(env);
}

int
main(int argc, char *argv[])
{
	ft_t env = ft_new();
	ft_create(env, test_waiter, env);
	ft_start(env);
	ft_free(env);
	return 0;
}
