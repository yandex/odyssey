
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <uv.h>
#include <assert.h>

static void
test_connect(void *arg)
{
	machine_t machine = arg;
	printf("fiber started\n");

	machine_io_t client = machine_create_io(machine);
	machine_tls_t tls = machine_create_tls(machine);
	machine_set_tls(client, tls);

	struct sockaddr_in sa;
	uv_ip4_addr("127.0.0.1", 44330, &sa);
	int rc;
	rc = machine_connect(client, (struct sockaddr*)&sa, 0);
	printf("connect: %d\n", rc);

	machine_close(client);
}

int
main(int argc, char *argv[])
{
	machine_t machine = machine_create();
	machine_create_fiber(machine, test_connect, machine);
	machine_start(machine);
	printf("shutting down\n");
	machine_free(machine);
	return 0;
}
