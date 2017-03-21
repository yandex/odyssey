
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
	char msg[] = "hello world" "HELLO WORLD" "a" "b" "c" "333";
	rc = machine_write(client, msg, sizeof(msg), 0);
	if (rc < 0) {
		printf("server: write error.\n");
		machine_close(client);
		machine_close(server);
		return;
	}


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

	/* read and fill readahead buffer */
	rc = machine_read(client, 11, 0);
	if (rc < 0) {
		printf("client: read failed\n");
		machine_close(client);
		return;
	}

	char *buf = machine_read_buf(client);
	assert(memcmp(buf, "hello world", 11) == 0);

	/* read from buffer */
	rc = machine_read(client, 11, 0);
	assert(rc == 0);
	buf = machine_read_buf(client);
	assert(memcmp(buf, "HELLO WORLD", 11) == 0);

	rc = machine_read(client, 1, 0);
	assert(rc == 0);
	buf = machine_read_buf(client);
	assert(*buf == 'a');

	rc = machine_read(client, 1, 0);
	assert(rc == 0);
	buf = machine_read_buf(client);
	assert(*buf == 'b');

	rc = machine_read(client, 1, 0);
	assert(rc == 0);
	buf = machine_read_buf(client);
	assert(*buf == 'c');

	rc = machine_read(client, 3, 0);
	assert(rc == 0);
	buf = machine_read_buf(client);
	assert(memcmp(buf, "333", 3) == 0);

	/* eof */
	rc = machine_read(client, 1, 0);
	if (rc < 0) {
	}

	machine_close(client);

	printf("client: done\n");
	machine_stop(machine);
}

int
main(int argc, char *argv[])
{
	machine_t machine = machine_create();
	machine_create_fiber(machine, server, machine);
	machine_create_fiber(machine, client, machine);
	machine_start(machine);
	machine_free(machine);
	return 0;
}
