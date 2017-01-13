
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <assert.h>

static void
fiber_1(void *arg)
{
	mm_t env = arg;
	mm_io_t io = mm_io_new(env);
	struct addrinfo *res = NULL;
	int rc = mm_getaddrinfo(io, "localhost", "http", NULL, &res, 0);
	if (rc < 0) {
		printf("failed to resolve address\n");
	} else {
		assert(res != NULL);
	}
	if (res)
		freeaddrinfo(res);
	mm_close(io);
}

static void
fiber_2(void *arg)
{
	mm_t env = arg;
	mm_io_t io = mm_io_new(env);
	struct addrinfo *res = NULL;
	int rc = mm_getaddrinfo(io, "localhost", "http", NULL, &res, 0);
	if (rc < 0) {
		printf("failed to resolve address\n");
	} else {
		assert(res != NULL);
	}
	if (res)
		freeaddrinfo(res);
	mm_close(io);
}

int
main(int argc, char *argv[])
{
	mm_t env = mm_new();
	mm_create(env, fiber_1, env);
	mm_create(env, fiber_2, env);
	mm_start(env);
	mm_free(env);
	return 0;
}
