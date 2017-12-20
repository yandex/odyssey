
/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#include <machinarium.h>
#include <shapito.h>

#include "histogram.h"

typedef struct {
	int              id;
	machine_io_t    *io;
	shapito_stream_t stream;
	int              coroutine_id;
	int              processed;
} stress_client_t;

typedef struct {
	char *dbname;
	char *user;
	char *host;
	char *port;
	int   time_to_run;
	int   clients;
} stress_t;

static stress_t       stress;
static od_histogram_t stress_histogram;
static int            stress_run;

static inline int
stress_read(machine_io_t *io, shapito_stream_t *stream)
{
	uint32_t request_start = shapito_stream_used(stream);
	uint32_t request_size = 0;
	for (;;)
	{
		char *request_data = stream->start + request_start;
		uint32_t len;
		int to_read;
		to_read = shapito_read(&len, &request_data, &request_size);
		if (to_read == 0)
			break;
		if (to_read == -1)
			return -1;
		int rc;
		rc = shapito_stream_ensure(stream, to_read);
		if (rc == -1)
			return -1;
		rc = machine_read(io, stream->pos, to_read, UINT32_MAX);
		if (rc == -1)
			return -1;
		shapito_stream_advance(stream, to_read);
		request_size += to_read;
	}
	return request_start;
}

static inline void
stress_client_main(void *arg)
{
	stress_client_t *client = arg;

	/* create client io */
	client->io = machine_io_create();
	if (client->io == NULL) {
		printf("client %d: failed to create io\n", client->id);
		return;
	}
	machine_set_nodelay(client->io, 1);
	machine_set_keepalive(client->io, 1, 7200);

	/* resolve host */
	struct addrinfo *ai = NULL;
	int rc;
	rc = machine_getaddrinfo(stress.host, stress.port, NULL, &ai, UINT32_MAX);
	if (rc == -1) {
		printf("client %d: failed to resolve host\n", client->id);
		return;
	}

	/* connect */
	rc = machine_connect(client->io, ai->ai_addr, UINT32_MAX);
	freeaddrinfo(ai);
	if (rc == -1) {
		printf("client %d: failed to connect\n", client->id);
		return;
	}

	printf("client %d: connected\n", client->id);

	/* handle client startup */
	shapito_stream_t *stream = &client->stream;
	shapito_fe_arg_t argv[] = {
		{ "user", 5 },
		{ stress.user, strlen(stress.user) + 1 },
		{ "database", 9 },
		{ stress.dbname, strlen(stress.dbname) + 1}
	};
	rc = shapito_fe_write_startup_message(stream, 4, argv);
	if (rc == -1)
		return;
	rc = machine_write(client->io, stream->start, shapito_stream_used(stream),
	                   UINT32_MAX);
	if (rc == -1) {
		printf("client %d: write error: %s\n", client->id,
		       machine_error(client->io));
		return;
	}

	int is_ready = 0;
	while (! is_ready)
	{
		shapito_stream_reset(stream);
		int rc;
		rc = stress_read(client->io, stream);
		if (rc == -1) {
			printf("client %d: read error: %s\n", client->id,
			       machine_error(client->io));
			return;
		}
		char type = *stream->start;
		if (type == 'E') {
			return;
		}
		if (type == 'Z') {
			break;
		}
	}

	printf("client %d: ready\n", client->id);

	char query[] = "SELECT 1;";

	/* oltp */
	while (stress_run)
	{
		shapito_stream_reset(stream);

		int start_time = od_histogram_time_us();

		/* request */
		rc = shapito_fe_write_query(stream, query, sizeof(query));
		if (rc == -1) {
			return;
		}
		rc = machine_write(client->io, stream->start, shapito_stream_used(stream),
		                   UINT32_MAX);
		if (rc == -1) {
			printf("client %d: write error: %s\n", client->id,
			       machine_error(client->io));
			return;
		}
		/* reply */
		for (;;) {
			shapito_stream_reset(stream);
			int rc;
			rc = stress_read(client->io, stream);
			if (rc == -1) {
				printf("client %d: read error: %s\n", client->id,
				       machine_error(client->io));
				return;
			}
			char type = *stream->start;
			if (type == 'E') {
				break;
			}
			if (type == 'Z') {
				int execution_time = od_histogram_time_us() - start_time;
				od_histogram_add(&stress_histogram, execution_time);
				client->processed++;
				break;
			}
		}
	}

	/* finish */
	shapito_stream_reset(stream);
	rc = shapito_fe_write_terminate(stream);
	if (rc == -1)
		return;
	rc = machine_write(client->io, stream->start, shapito_stream_used(stream),
	                   UINT32_MAX);
	if (rc == -1) {
		printf("client %d: write error: %s\n", client->id,
		       machine_error(client->io));
		return;
	}
	machine_close(client->io);
	printf("client %d: done (%d processed)\n", client->id, client->processed);
}

static inline void
stress_main(void *arg)
{
	stress_t *stress = arg;

	stress_client_t *clients;
	clients = calloc(stress->clients, sizeof(stress_client_t));
	if (clients == NULL)
		return;

	stress_run = 1;

	/* create workers */
	int i = 0;
	for (; i < stress->clients; i++) {
		stress_client_t *client = &clients[i];
		client->id = i;
		shapito_stream_init(&client->stream);
		client->coroutine_id = machine_coroutine_create(stress_client_main, client);
	}

	/* give time for work */
	machine_sleep(stress->time_to_run * 1000);

	stress_run = 0;

	/* wait for completion and calculate stats */
	for (i = 0; i < stress->clients; i++) {
		stress_client_t *client = &clients[i];
		machine_join(client->coroutine_id);
		if (client->io)
			machine_io_free(client->io);
		shapito_stream_free(&client->stream);
	}
	free(clients);

	/* result */
	od_histogram_print(&stress_histogram, stress->clients, stress->time_to_run);
}

int main(int argc, char *argv[])
{
	od_histogram_init(&stress_histogram);
	memset(&stress, 0, sizeof(stress));
	stress_run = 0;
	char *user = getenv("USER");
	if (user == NULL)
		user = "test";
	stress.user = user;
	stress.dbname = user;
	stress.host = "localhost";
	stress.port = "6432";
	stress.time_to_run = 5;
	stress.clients = 10;

	int opt;
	while ((opt = getopt(argc, argv, "d:u:h:p:t:c:")) != -1) {
		switch (opt) {
		/* database */
		case 'd':
			stress.dbname = optarg;
			break;
		/* user */
		case 'u':
			stress.user = optarg;
			break;
		/* host */
		case 'h':
			stress.host = optarg;
			break;
		/* port */
		case 'p':
			stress.port = optarg;
			break;
		/* time */
		case 't':
			stress.time_to_run = atoi(optarg);
			break;
		/* clients */
		case 'c':
			stress.clients = atoi(optarg);
			break;
		default:
			printf("PostgreSQL benchmarking.\n\n");
			printf("usage: %s [duhptc]\n", argv[0]);
			printf("  \n");
			printf("  -d <database>   database name\n");
			printf("  -u <user>       user name\n");
			printf("  -h <host>       server address\n");
			printf("  -p <port>       server port\n");
			printf("  -t <time>       time to run (seconds)\n");
			printf("  -c <clients>    number of clients\n");
			return 1;
		}
	}

	printf("PostgreSQL benchmarking.\n\n");
	printf("time to run: %d secs\n", stress.time_to_run);
	printf("clients:     %d\n", stress.clients);
	printf("database:    %s\n", stress.dbname);
	printf("user:        %s\n", stress.user);
	printf("host:        %s\n", stress.host);
	printf("port:        %s\n", stress.port);
	printf("\n");

	machinarium_init();

	int64_t machine;
	machine = machine_create("stresser", stress_main, &stress);

	machine_wait(machine);

	machinarium_free();
	return 0;
}
