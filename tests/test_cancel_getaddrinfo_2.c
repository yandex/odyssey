
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <assert.h>

static void
test_gai(void *arg)
{
	mm_t env = arg;
	assert(mm_is_cancel(env));
	printf("child started\n");
	mm_io_t io = mm_io_new(env);
	struct addrinfo *res = NULL;
	int rc = mm_getaddrinfo(io, "abracadabra", "http", NULL, &res, 0);
	assert(rc < 0);
	mm_close(io);
	assert(res == NULL);
	printf("child done\n");
}

static void
test_waiter(void *arg)
{
	mm_t env = arg;

	printf("waiter started\n");

	int id = mm_create(env, test_gai, env);
	mm_cancel(env, id); /* run cancelled */
	mm_sleep(env, 0);
	mm_wait(env, id);

	printf("waiter ended \n");
	mm_stop(env);
}

int
main(int argc, char *argv[])
{
	mm_t env = mm_new();
	mm_create(env, test_waiter, env);
	mm_start(env);
	printf("shutting down\n");
	mm_free(env);
	return 0;
}
