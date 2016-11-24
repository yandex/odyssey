
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
	printf("sleep 10 ms\n");
	mm_sleep(env, 10);
	printf("sleep wakeup\n");
	printf("child ended\n");
	mm_stop(env);
}

int
main(int argc, char *argv[])
{
	mm_t env = mm_new();
	mm_create(env, test_child, env);
	mm_start(env);
	mm_free(env);
	return 0;
}
