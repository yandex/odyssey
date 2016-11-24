
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
	mm_t env = arg;
	printf("child 0 started\n");
	mm_sleep(env, 1000);
	printf("child 0 ended\n");
}

static void
test_child_1(void *arg)
{
	mm_t env = arg;
	printf("child 1 started\n");
	mm_sleep(env, 500);
	printf("child 1 ended\n");
}

static void
test_waiter(void *arg)
{
	mm_t env = arg;

	printf("waiter started\n");

	int a = mm_create(env, test_child_0, env);
	int b = mm_create(env, test_child_1, env);

	mm_wait(env, b);
	printf("waiter 1 ended \n");
	mm_wait(env, a);
	printf("waiter 0 ended \n");

	assert( mm_wait(env, a) == -1 );
	assert( mm_wait(env, b) == -1 );

	printf("waiter ended\n");
	mm_stop(env);
}

int
main(int argc, char *argv[])
{
	mm_t env = mm_new();
	mm_create(env, test_waiter, env);
	mm_start(env);
	mm_free(env);
	return 0;
}
