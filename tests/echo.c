
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

/*
 * This example shows basic tcp server.
 * Read 10 bytes with 5 sec timeout then write back.
 *
 * echo 0123456789 | nc -v 127.0.0.1 7778 # to test
*/

#include <machinarium.h>
#include <uv.h>

static void
test_client(void *arg)
{
	mm_io_t client = arg;
	int fd = mm_io_fd(client);
	printf("new connection: %d \n", fd);
	for (;;) {
		int rc;
		/* read 10 bytes (with 5 sec timeout) */
		rc = mm_read(client, 10, 5 * 1000);
		if (rc < 0) {
			if (mm_read_is_timeout(client)) {
				printf("timeout in %d\n", fd);
				continue;
			}
			if (! mm_is_connected(client)) {
				printf("client disconnected\n");
				break;
			}
			printf("client read error\n");
		}
		/* write 10 bytes */
		char *buf = mm_read_buf(client);
		rc = mm_write(client, buf, 10, 0);
		if (rc < 0)
			printf("client write error\n");
	}
	printf("client %d disconnected\n", fd);
	mm_close(client);
}

static void
test_server(void *arg)
{
	mm_t env = arg;
	mm_io_t server = mm_io_new(env);

	struct sockaddr_in sa;
	uv_ip4_addr("127.0.0.1", 7778, &sa);
	int rc;
	rc = mm_bind(server, (struct sockaddr*)&sa);
	if (rc < 0) {
		printf("bind failed\n");
		mm_close(server);
		return;
	}

	printf("waiting for connections (127.0.0.1:7778)\n");
	for (;;) {
		mm_io_t client;
		rc = mm_accept(server, 16, &client);
		if (rc < 0) {
			printf("accept error.\n");
		} else
		if (rc == 0) {
			mm_create(env, test_client, client);
		}
	}

	/* unreach */
	mm_close(server);
}

int
main(int argc, char *argv[])
{
	mm_t env = mm_new();
	mm_create(env, test_server, env);
	mm_start(env);
	mm_free(env);
	return 0;
}
