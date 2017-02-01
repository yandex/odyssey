
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
	mm_t env = arg;
	printf("condition fiber started\n");
	int rc = mm_condition(env, 1000);
	assert(rc == 0);
	printf("condition fiber ended\n");
}

static void
test_waiter(void *arg)
{
	mm_t env = arg;

	printf("waiter started\n");

	int a = mm_create(env, test_condition, env);

	mm_sleep(env, 0);
	mm_signal(env, a);
	mm_sleep(env, 0);

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
