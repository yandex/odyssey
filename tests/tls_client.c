
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
		machine_tls_set_cert_file(tls, key_file);

	/* client */
	machine_io_t client;
	client = machine_create_io(machine);
	machine_set_tls(client, tls);

	/* connect */
	rc = machine_connect(client, ai->ai_addr, 0);
	freeaddrinfo(ai);
	if (rc == -1) {
		char *error;
		error = machine_error(client);
		printf("machine_connect(): %s\n", error);
	} else {
		printf("connected to %s:%s\n", addr, port);
	}

	for (;;) {
		char buf[256];
		int  len;
		char *p = fgets(buf, sizeof(buf), stdin);
		if (! p)
			break;
		len = strlen(buf);
		rc = machine_write(client, buf, len, 0);
		if (rc == -1) {
			char *error;
			error = machine_error(client);
			printf("machine_write(): %s\n", error);
		}
	}

	machine_close(client);
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
			       "       [-a address]\n"
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
