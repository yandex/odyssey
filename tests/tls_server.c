
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

static char *verify    = NULL;
static char *server    = NULL;
static char *ca_path   = NULL;
static char *ca_file   = NULL;
static char *cert_file = NULL;
static char *key_file  = NULL;
static char *addr      = "127.0.0.1";
static char *port      = "44330";

static void
test_client(void *arg)
{
	machine_io_t client = arg;
	int fd = machine_fd(client);
	printf("new connection: %d \n", fd);
	for (;;) {
		int rc;
		char buf[10];
		/* read 10 bytes (with 5 sec timeout) */
		rc = machine_read(client, buf, 10, 0);
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
			char *error;
			error = machine_error(client);
			printf("machine_read(): %s\n", error);
			break;
		}
		/* write 10 bytes */
		rc = machine_write(client, buf, 10, 0);
		if (rc < 0)
			printf("client write error\n");
	}
	machine_close(client);
}

static void
test_connect(void *arg)
{
	machine_t machine = arg;
	printf("fiber started\n");

	/* resolve */
	struct addrinfo *ai = NULL;
	machine_io_t resolver;
	resolver = machine_create_io(machine);
	int rc;
	rc = machine_getaddrinfo(resolver, addr, port, NULL, &ai, 0);
	if (rc == -1) {
		char *error;
		error = machine_error(resolver);
		printf("machine_getaddrinfo(): %s\n", error);
		machine_close(resolver);
		machine_stop(machine);
		return;
	}
	machine_close(resolver);

	/* tls context */
	machine_tls_t tls;
	tls = machine_create_tls(machine);
	if (verify)
		machine_tls_set_verify(tls, verify);
	if (server)
		machine_tls_set_server(tls, server);
	if (ca_path)
		machine_tls_set_ca_path(tls, ca_path);
	if (ca_file)
		machine_tls_set_ca_file(tls, ca_file);
	if (cert_file)
		machine_tls_set_cert_file(tls, cert_file);
	if (key_file)
		machine_tls_set_key_file(tls, key_file);

	/* server */
	machine_io_t server;
	server = machine_create_io(machine);

	/* bind */
	rc = machine_bind(server, ai->ai_addr);
	if (rc < 0) {
		char *error;
		error = machine_error(server);
		printf("machine_bind(): %s\n", error);
		machine_close(server);
		machine_stop(machine);
		return;
	}
	machine_set_tls(server, tls);

	printf("waiting for connections (%s:%s)\n", addr, port);
	for (;;) {
		machine_io_t client;
		rc = machine_accept(server, 16, &client);
		if (rc < 0) {
			char *error;
			error = machine_error(server);
			printf("machine_accept(): %s\n", error);
		} else
		if (rc == 0) {
			machine_create_fiber(machine, test_client, client);
		}
	}
	machine_close(server);
	machine_stop(machine);
}

int
main(int argc, char *argv[])
{
	int opt;
	while ((opt = getopt(argc, argv, "hv:k:c:r:R:s:a:p:")) != -1) {
		switch (opt) {
		case 'h':
			printf("usage: [-v verify_mode]\n"
			       "       [-k key_file]\n"
			       "       [-c cert_file]\n"
			       "       [-r ca_file ]\n"
			       "       [-R ca_path]\n"
			       "       [-s server_name]\n"
			       "       [-a listen_address]\n"
			       "       [-p port]\n");
			return 0;
		case 'v':
			verify = optarg;
			break;
		case 'k':
			key_file = optarg;
			break;
		case 'c':
			cert_file = optarg;
			break;
		case 'r':
			ca_file = optarg;
			break;
		case 'R':
			ca_path = optarg;
			break;
		case 's':
			server = optarg;
			break;
		case 'a':
			addr = optarg;
			break;
		case 'p':
			port = optarg;
			break;
		}
	}
	machine_t machine = machine_create();
	machine_create_fiber(machine, test_connect, machine);
	machine_start(machine);
	machine_free(machine);
	return 0;
}
