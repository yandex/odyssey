
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

	machine_t machine = arg;
	machine_io_t server = machine_create_io(machine);

	struct sockaddr_in sa;
	uv_ip4_addr("127.0.0.1", 7778, &sa);

	int rc;
	rc = machine_bind(server, (struct sockaddr*)&sa);
	if (rc < 0) {
		printf("server: bind failed\n");
		machine_close(server);
		return;
	}

	printf("server: waiting for connections (127.0.0.1:7778)\n");
	machine_io_t client;
	rc = machine_accept(server, 16, &client);
	if (rc < 0) {
		printf("accept error.\n");
		machine_close(server);
		return;
	}

	machine_sleep(machine, 100 * 1000);

	machine_close(client);
	machine_close(server);
	printf("server: done\n");
}

static void
client(void *arg)
{
	printf("client: started\n");

	machine_t machine = arg;
	machine_io_t client = machine_create_io(machine);

	struct sockaddr_in sa;
	uv_ip4_addr("127.0.0.1", 7778, &sa);

	int rc;
	rc = machine_connect(client, (struct sockaddr*)&sa, 0);
	if (rc < 0) {
		printf("client: connect failed\n");
		machine_close(client);
		return;
	}

	printf("client: connected\n");

	/* will wait forever */
	rc = machine_read(client, 12, 0);
	if (rc < 0) {
		if (machine_cancelled(machine)) {
			printf("client: read cancelled\n");
		} else {
			printf("client: read failed\n");
		}
		machine_close(client);
		return;
	}

	machine_close(client);

	printf("client: done\n");
}

static void
runner(void *arg)
{
	machine_t machine = arg;

	int server_id = machine_create_fiber(machine, server, machine);
	int client_id = machine_create_fiber(machine, client, machine);

	/* switch to server, client */
	machine_sleep(machine, 0);

	/* switch to client read */
	machine_sleep(machine, 0);

	/* cancel client read */
	machine_cancel(machine, client_id);

	machine_sleep(machine, 0);
	machine_cancel(machine, server_id);
	machine_sleep(machine, 0);

	machine_stop(machine);
}

int
main(int argc, char *argv[])
{
	machine_t machine = machine_create();
	machine_create_fiber(machine, runner, machine);
	machine_start(machine);
	machine_free(machine);
	return 0;
}
