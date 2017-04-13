
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>
#include <string.h>
#include <assert.h>

static void
server(void *arg)
{
	printf("server: started\n");

	machine_t machine = arg;
	machine_io_t server = machine_create_io(machine);

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(7778);

	int rc;
	rc = machine_bind(server, (struct sockaddr*)&sa);
	if (rc < 0) {
		printf("server: bind failed\n");
		machine_close(server);
		machine_stop(machine);
		return;
	}

	printf("server: waiting for connections (127.0.0.1:7778)\n");
	machine_io_t client;
	rc = machine_accept(server, &client, 16, 500);
	if (rc < 0) {
		printf("accept error: %s\n", machine_error(server));
		machine_close(server);
		machine_free_io(server);
		machine_stop(machine);
		return;
	}
	char msg[] = "hello world";
	rc = machine_write(client, msg, sizeof(msg), 0);
	if (rc < 0) {
		printf("server: write error: %s\n", machine_error(client));
		machine_close(server);
		machine_free_io(server);
		machine_stop(machine);
		return;
	}

	machine_close(client);
	machine_free_io(client);
	machine_close(server);
	machine_free_io(server);

	printf("server: done\n");

	machine_stop(machine);
}

int
main(int argc, char *argv[])
{
	machine_t machine = machine_create();
	machine_create_fiber(machine, server, machine);
	machine_start(machine);
	machine_free(machine);
	return 0;
}
