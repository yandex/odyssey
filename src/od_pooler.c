
/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <signal.h>

#include <machinarium.h>
#include <soprano.h>

#include "od_macro.h"
#include "od_version.h"
#include "od_list.h"
#include "od_pid.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_daemon.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"
#include "od_msg.h"
#include "od_system.h"
#include "od_instance.h"
#include "od_server.h"
#include "od_server_pool.h"
#include "od_client.h"
#include "od_client_pool.h"
#include "od_route_id.h"
#include "od_route.h"
#include "od_pooler.h"

static inline void
od_pooler(void *arg)
{
	od_pooler_t *pooler = arg;
	od_instance_t *instance = pooler->system->instance;

	od_log(&instance->log, "pooler: started");

	/* init pooler tls */
	int rc;
#if 0
	pooler->tls = NULL;
	od_scheme_t *scheme = &pooler->instance->scheme;
	if (scheme->tls_verify != OD_TDISABLE) {
		pooler->tls = od_tlsfe(pooler->env, scheme);
		if (pooler->tls == NULL)
			return;
	}
#endif

	/* listen '*' */
	struct addrinfo *hints_ptr = NULL;
	struct addrinfo  hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;
	char *host = instance->scheme.host;
	if (strcmp(instance->scheme.host, "*") == 0) {
		hints_ptr = &hints;
		host = NULL;
	}

	/* resolve listen address and port */
	char port[16];
	snprintf(port, sizeof(port), "%d", instance->scheme.port);
	struct addrinfo *ai = NULL;
	rc = machine_getaddrinfo(host, port, hints_ptr, &ai, UINT32_MAX);
	if (rc < 0) {
		od_error(&instance->log, "failed to resolve %s:%d",
		          instance->scheme.host,
		          instance->scheme.port);
		return;
	}
	assert(ai != NULL);

	/* io */
	pooler->server = machine_io_create();
	if (pooler->server == NULL) {
		od_error(&instance->log, "failed to create pooler io");
		return;
	}

	/* bind to listen address and port */
	rc = machine_bind(pooler->server, ai->ai_addr);
	freeaddrinfo(ai);
	if (rc < 0) {
		od_error(&instance->log, "bind %s:%d failed",
		          instance->scheme.host,
		          instance->scheme.port);
		return;
	}

	od_log(&instance->log, "pooler: listening on %s:%d",
	        instance->scheme.host,
	        instance->scheme.port);
	od_log(&instance->log, "");

	/* main loop */
	while (machine_active())
	{
		machine_io_t client_io;
		rc = machine_accept(pooler->server, &client_io,
		                    instance->scheme.backlog, UINT32_MAX);
		if (rc < 0) {
			od_error(&instance->log, "pooler: accept failed");
			continue;
		}
		/* todo: client_max limit */

		/* set network options */
		machine_set_nodelay(client_io, instance->scheme.nodelay);
		if (instance->scheme.keepalive > 0)
			machine_set_keepalive(client_io, 1, instance->scheme.keepalive);
		rc = machine_set_readahead(client_io, instance->scheme.readahead);
		if (rc == -1) {
			od_error(&instance->log, "failed to set client readahead");
			machine_close(client_io);
			machine_io_free(client_io);
			continue;
		}

		/* detach io from pooler event loop */
		rc = machine_io_detach(client_io);
		if (rc == -1) {
			od_error(&instance->log, "failed to transfer client io");
			machine_close(client_io);
			machine_io_free(client_io);
			continue;
		}

		/* allocate new client */
		od_client_t *client = od_client_allocate();
		if (client == NULL) {
			od_error(&instance->log, "failed to allocate client object");
			machine_close(client_io);
			machine_io_free(client_io);
			continue;
		}
		client->id = pooler->client_seq++;
		client->io = client_io;

		/* create new client event */
		machine_msg_t msg;
		msg = machine_msg_create(OD_MCLIENT_NEW, sizeof(od_client_t*));
		char *msg_data = machine_msg_get_data(msg);
		memcpy(msg_data, &client, sizeof(od_client_t*));
		machine_queue_put(pooler->system->task_queue, msg);
	}
}

int od_pooler_init(od_pooler_t *pooler, od_system_t *system)
{
	pooler->machine = -1;
	pooler->server = NULL;
	pooler->client_seq = 0;
	pooler->system = system;
	return 0;
}

int od_pooler_start(od_pooler_t *pooler)
{
	od_instance_t *instance = pooler->system->instance;
	pooler->machine = machine_create("pooler", od_pooler, pooler);
	if (pooler->machine == -1) {
		od_error(&instance->log, "failed to start server");
		return 1;
	}
	return 0;
}
