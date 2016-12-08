
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <uv.h>
#include <string.h>
#include <assert.h>

static void
server(void *arg)
{
	printf("server: started\n");

	mm_t env = arg;
	mm_io_t server = mm_io_new(env);

	struct sockaddr_in sa;
	uv_ip4_addr("127.0.0.1", 7778, &sa);

	int rc;
	rc = mm_bind(server, (struct sockaddr*)&sa);
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
	char msg[] = "hello world" "HELLO WORLD" "a" "b" "c" "333";
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

	struct sockaddr_in sa;
	uv_ip4_addr("127.0.0.1", 7778, &sa);

	int rc;
	rc = mm_connect(client, (struct sockaddr*)&sa, 0);
	if (rc < 0) {
		printf("client: connect failed\n");
		mm_close(client);
		return;
	}

	printf("client: connected\n");

	/* read and fill readahead buffer */
	rc = mm_read(client, 11, 0);
	if (rc < 0) {
		printf("client: read failed\n");
		mm_close(client);
		return;
	}

	char *buf = mm_read_buf(client);
	assert(memcmp(buf, "hello world", 11) == 0);

	/* read from buffer */
	rc = mm_read(client, 11, 0);
	assert(rc == 0);
	buf = mm_read_buf(client);
	assert(memcmp(buf, "HELLO WORLD", 11) == 0);

	rc = mm_read(client, 1, 0);
	assert(rc == 0);
	buf = mm_read_buf(client);
	assert(*buf == 'a');

	rc = mm_read(client, 1, 0);
	assert(rc == 0);
	buf = mm_read_buf(client);
	assert(*buf == 'b');

	rc = mm_read(client, 1, 0);
	assert(rc == 0);
	buf = mm_read_buf(client);
	assert(*buf == 'c');

	rc = mm_read(client, 3, 0);
	assert(rc == 0);
	buf = mm_read_buf(client);
	assert(memcmp(buf, "333", 3) == 0);

	/* eof */
	rc = mm_read(client, 1, 0);
	if (rc < 0) {
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
