
/*
 * fluent.
 *
 * Cooperative multitasking engine.
*/

/*
 * This example shows basic tcp server.
 * Read 10 bytes with 5 sec timeout then write back.
 *
 * echo 0123456789 | nc -v 127.0.0.1 7778 # to test
*/

#include <fluent.h>

static void
test_client(void *arg)
{
	ftio_t client = arg;
	int fd = ft_fd(client);
	printf("new connection: %d \n", fd);
	for (;;) {
		int rc;
		/* read 10 bytes (with 5 sec timeout) */
		rc = ft_read(client, 10, 5 * 1000);
		if (rc < -1) {
			printf("client read error\n");
			if (ft_read_is_timeout(client)) {
				printf("timeout in %d\n", fd);
				continue;
			}
			if (! ft_is_connected(client)) {
				printf("client disconnected\n");
				break;
			}
		}
		/* write 10 bytes */
		char *buf = ft_read_buf(client);
		rc = ft_write(client, buf, 10, 0);
		if (rc < -1)
			printf("client write error\n");
	}
	printf("client %d disconnected\n", fd);
	ft_close(client);
}

static void
test_server(void *arg)
{
	ft_t env = arg;
	ftio_t server = ft_io_new(env);

	int rc;
	rc = ft_bind(server, "127.0.0.1", 7778);
	if (rc == -1) {
		printf("bind failed\n");
		ft_close(server);
		return;
	}

	printf("waiting for connections (127.0.0.1:7778)\n");
	for (;;) {
		ftio_t client;
		rc = ft_accept(server, &client);
		if (rc == -1) {
			printf("accept error.\n");
		} else
		if (rc == 0) {
			ft_create(env, test_client, client);
		}
	}

	/* unreach */
	ft_close(server);
}

int
main(int argc, char *argv[])
{
	ft_t env = ft_new();
	ft_create(env, test_server, env);
	ft_start(env);
	ft_close(env);
	return 0;
}
