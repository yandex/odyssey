
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <assert.h>

static void
test_child(void *arg)
{
	mm_t env = arg;
	printf("child started\n");
	printf("child sleep for 600 seconds\n");
	mm_sleep(env, 600 * 1000);
	printf("child wakeup\n");
	if (mm_is_cancel(env))
		printf("child cancelled\n");
	printf("child 0 ended\n");
}

static void
test_parent(void *arg)
{
	mm_t env = arg;

	printf("parent started\n");

	int id = mm_create(env, test_child, env);
	mm_sleep(env, 0);
	mm_cancel(env, id);
	mm_wait(env, id);
	assert( mm_wait(env, id) == -1 );

	printf("parent ended\n");
	mm_stop(env);
}

int
main(int argc, char *argv[])
{
	mm_t env = mm_new();
	mm_create(env, test_parent, env);
	mm_start(env);
	mm_free(env);
	return 0;
}
