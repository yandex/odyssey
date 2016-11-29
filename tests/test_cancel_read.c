
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <string.h>
#include <assert.h>

static void
server(void *arg)
{
	printf("server: started\n");

	mm_t env = arg;
	mm_io_t server = mm_io_new(env);

	int rc;
	rc = mm_bind(server, "127.0.0.1", 7778);
	if (rc < 0) {
		printf("server: bind failed\n");
		mm_close(server);
		return;
	}

	printf("server: waiting for connections (127.0.0.1:7778)\n");
	mm_io_t client;
	rc = mm_accept(server, 16, &client);
	if (rc < 0) {
		printf("accept error.\n");
		mm_close(server);
		return;
	}

	mm_sleep(env, 100 * 1000);

	mm_close(client);
	mm_close(server);
	printf("server: done\n");
}

static void
client(void *arg)
{
	printf("client: started\n");

	mm_t env = arg;
	mm_io_t client = mm_io_new(env);

	int rc;
	rc = mm_connect(client, "127.0.0.1", 7778, 0);
	if (rc < 0) {
		printf("client: connect failed\n");
		mm_close(client);
		return;
	}

	printf("client: connected\n");

	/* will wait forever */
	rc = mm_read(client, 12, 0);
	if (rc < 0) {
		if (mm_is_cancel(env)) {
			printf("client: read cancelled\n");
		} else {
			printf("client: read failed\n");
		}
		mm_close(client);
		return;
	}

	mm_close(client);

	printf("client: done\n");
}

static void
runner(void *arg)
{
	mm_t env = arg;

	int server_id = mm_create(env, server, env);
	int client_id = mm_create(env, client, env);

	/* switch to server, client */
	mm_sleep(env, 0);

	/* switch to client read */
	mm_sleep(env, 0);

	/* cancel client read */
	mm_cancel(env, client_id);

	mm_sleep(env, 0);
	mm_cancel(env, server_id);
	mm_sleep(env, 0);

	mm_stop(env);
}

int
main(int argc, char *argv[])
{
	mm_t env = mm_new();
	mm_create(env, runner, env);
	mm_start(env);
	mm_free(env);
	return 0;
}
