
/*
 * flint.
 *
 * Cooperative multitasking engine.
*/

#include <flint.h>
#include <assert.h>

static void
test_child_0(void *arg)
{
	ft_t env = arg;
	printf("child 0 started\n");
	ft_sleep(env, 1000);
	printf("child 0 ended\n");
}

static void
test_child_1(void *arg)
{
	ft_t env = arg;
	printf("child 1 started\n");
	ft_sleep(env, 500);
	printf("child 1 ended\n");
}

static void
test_waiter(void *arg)
{
	ft_t env = arg;

	printf("waiter started\n");

	int a = ft_create(env, test_child_0, env);
	int b = ft_create(env, test_child_1, env);

	ft_wait(env, b);
	printf("waiter 1 ended \n");
	ft_wait(env, a);
	printf("waiter 0 ended \n");

	assert( ft_wait(env, a) == -1 );
	assert( ft_wait(env, b) == -1 );

	printf("waiter ended\n");
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
