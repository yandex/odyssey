
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
	machine_io_t client = arg;
	int fd = machine_fd(client);
	printf("new connection: %d \n", fd);
	for (;;) {
		int rc;
		/* read 10 bytes (with 5 sec timeout) */
		rc = machine_read(client, NULL, 10, 5 * 1000);
		if (rc < 0) {
			if (machine_read_timedout(client)) {
				printf("timeout in %d\n", fd);
				continue;
			}
			if (! machine_connected(client)) {
				printf("client disconnected\n");
				break;
			}
			printf("client read error\n");
		}
		/* write 10 bytes */
		char *buf = machine_read_buf(client);
		rc = machine_write(client, buf, 10, 0);
		if (rc < 0)
			printf("client write error\n");
	}
	printf("client %d disconnected\n", fd);
	machine_close(client);
}

static void
test_server(void *arg)
{
	machine_t machine = arg;
	machine_io_t server = machine_create_io(machine);

	struct sockaddr_in sa;
	uv_ip4_addr("127.0.0.1", 7778, &sa);
	int rc;
	rc = machine_bind(server, (struct sockaddr*)&sa);
	if (rc < 0) {
		printf("bind failed\n");
		machine_close(server);
		return;
	}

	printf("waiting for connections (127.0.0.1:7778)\n");
	for (;;) {
		machine_io_t client;
		rc = machine_accept(server, 16, &client);
		if (rc < 0) {
			printf("accept error.\n");
		} else
		if (rc == 0) {
			machine_create_fiber(machine, test_client, client);
		}
	}

	/* unreach */
	machine_close(server);
}

int
main(int argc, char *argv[])
{
	machine_t machine = machine_create();
	machine_create_fiber(machine, test_server, machine);
	machine_start(machine);
	machine_free(machine);
	return 0;
}
