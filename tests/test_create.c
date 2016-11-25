
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <assert.h>

int fiber_call = 0;

static void
fiber(void *arg)
{
	mm_t env = arg;
	fiber_call++;
	mm_stop(env);
}

int
main(int argc, char *argv[])
{
	mm_t env = mm_new();
	mm_create(env, fiber, env);
	mm_start(env);
	assert(fiber_call == 1);
	mm_free(env);
	return 0;
}
