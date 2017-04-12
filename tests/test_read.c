
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>
#include <assert.h>

static void
test_connect(void *arg)
{
	machine_t machine = arg;
	printf("child started\n");
	machine_io_t client = machine_create_io(machine);

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(33413);

	int rc;
	rc = machine_connect(client, (struct sockaddr *)&sa, 0);
	if (rc == -1)
		printf("connection failed: %s\n", machine_error(client));
	else
		printf("connected\n");

	rc = machine_write(client, "hello world", 11, 0);
	if (rc == -1)
		printf("write failed: %s\n", machine_error(client));

	char buf[10];
	rc = machine_read(client, buf, 10, 0);
	if (rc == -1)
		printf("read failed: %s\n", machine_error(client));
	
	printf("%.*s\n", 10, buf);

	machine_close(client);
	machine_free_io(client);

	printf("child end\n");
}

static void
test_waiter(void *arg)
{
	machine_t machine = arg;

	printf("waiter started\n");

	int id = machine_create_fiber(machine, test_connect, machine);
	machine_sleep(machine, 0);
	machine_wait(machine, id);

	printf("waiter 1 ended \n");
	machine_stop(machine);
}

int
main(int argc, char *argv[])
{
	machine_t machine = machine_create();
	machine_create_fiber(machine, test_waiter, machine);
	machine_start(machine);
	printf("shutting down\n");
	machine_free(machine);
	return 0;
}
