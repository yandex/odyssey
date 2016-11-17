
/*
 * flint.
 *
 * Cooperative multitasking engine.
*/

#include <flint.h>
#include <assert.h>
#include <errno.h>

static void
test_connect(void *arg)
{
	ft_t env = arg;
	printf("child started\n");
	ftio_t client = ft_io_new(env);
	int rc;
	rc = ft_connect(client, "8.8.8.16", 1324, 0);
	printf("child resumed\n");
	assert(rc < 0);
	ft_close(client);
	if (ft_is_cancel(env))
		printf("child marked as cancel\n");
	printf("child end\n");
}

static void
test_waiter(void *arg)
{
	ft_t env = arg;

	printf("waiter started\n");

	int id = ft_create(env, test_connect, env);
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
	printf("shutting down\n");
	ft_free(env);
	return 0;
}
