
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
	char msg[] = "hello world";
	rc = mm_write(client, msg, sizeof(msg), 0);
	if (rc < 0) {
		printf("server: write error.\n");
		mm_close(client);
		mm_close(server);
		return;
	}

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

	rc = mm_read(client, 12, 0);
	if (rc < 0) {
		printf("client: read failed\n");
		mm_close(client);
		return;
	}

	char *buf = mm_read_buf(client);
	assert(memcmp(buf, "hello world", 12) == 0);

	rc = mm_read(client, 1, 0);
	if (rc < 0) {
		/* eof */
	}

	mm_close(client);

	printf("client: done\n");
	mm_stop(env);
}

int
main(int argc, char *argv[])
{
	mm_t env = mm_new();
	mm_create(env, server, env);
	mm_create(env, client, env);
	mm_start(env);
	mm_free(env);
	return 0;
}
