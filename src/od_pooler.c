
/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <machinarium.h>
#include <shapito.h>

#include "od_macro.h"
#include "od_version.h"
#include "od_list.h"
#include "od_pid.h"
#include "od_id.h"
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
#include "od_route_pool.h"
#include "od_io.h"
#include "od_router.h"
#include "od_router.h"
#include "od_relay.h"
#include "od_relay_pool.h"
#include "od_pooler.h"
#include "od_periodic.h"
#include "od_tls.h"

static inline void
od_pooler_server(void *arg)
{
	od_poolerserver_t *server = arg;
	od_instance_t *instance = server->system->instance;
	od_relaypool_t *relay_pool = server->system->relay_pool;

	/* create server io */
	machine_io_t *server_io;
	server_io = machine_io_create();
	if (server_io == NULL) {
		od_error(&instance->log, "server", "failed to create pooler io");
		return;
	}

	char addr_name[128];
	od_getaddrname(server->addr, addr_name, sizeof(addr_name));

	/* bind to listen address and port */
	int rc;
	rc = machine_bind(server_io, server->addr->ai_addr);
	if (rc == -1) {
		od_error(&instance->log, "server", "bind to %s failed: %s",
		         addr_name,
		         machine_error(server_io));
		machine_close(server_io);
		machine_io_free(server_io);
		return;
	}

	od_log(&instance->log, "listening on %s", addr_name);

	/* main loop */
	for (;;)
	{
		machine_io_t *client_io;
		rc = machine_accept(server_io, &client_io,
		                    instance->scheme.backlog, UINT32_MAX);
		if (rc == -1) {
			od_error(&instance->log, "server", "accept failed: %s",
			         machine_error(server_io));
			int errno_ = machine_errno();
			if (errno_ == EADDRINUSE)
				break;
			continue;
		}

		/* set network options */
		machine_set_nodelay(client_io, instance->scheme.nodelay);
		if (instance->scheme.keepalive > 0)
			machine_set_keepalive(client_io, 1, instance->scheme.keepalive);
		rc = machine_set_readahead(client_io, instance->scheme.readahead);
		if (rc == -1) {
			od_error(&instance->log, "server", "failed to set client readahead: %s",
			         machine_error(client_io));
			machine_close(client_io);
			machine_io_free(client_io);
			continue;
		}

		/* detach io from pooler event loop */
		rc = machine_io_detach(client_io);
		if (rc == -1) {
			od_error(&instance->log, "server", "failed to transfer client io: %s",
			         machine_error(client_io));
			machine_close(client_io);
			machine_io_free(client_io);
			continue;
		}

		/* allocate new client */
		od_client_t *client = od_client_allocate();
		if (client == NULL) {
			od_error(&instance->log, "server", "failed to allocate client object");
			machine_close(client_io);
			machine_io_free(client_io);
			continue;
		}
		od_idmgr_generate(&instance->id_mgr, &client->id);
		client->io = client_io;

		/* create new client event and pass it to worker pool */
		machine_msg_t *msg;
		msg = machine_msg_create(OD_MCLIENT_NEW, sizeof(od_client_t*));
		char *msg_data = machine_msg_get_data(msg);
		memcpy(msg_data, &client, sizeof(od_client_t*));
		od_relaypool_feed(relay_pool, msg);
	}
}

static inline int
od_pooler_server_start(od_pooler_t *pooler, struct addrinfo *addr)
{
	od_instance_t *instance = pooler->system->instance;
	od_poolerserver_t *server;
	server = malloc(sizeof(od_poolerserver_t));
	if (server == NULL) {
		od_error(&instance->log, "pooler", "failed to allocate pooler server object");
		return -1;
	}
	server->addr = addr;
	server->system = pooler->system;
	int64_t coroutine_id;
	coroutine_id = machine_coroutine_create(od_pooler_server, server);
	if (coroutine_id == -1) {
		od_error(&instance->log, "pooler", "failed to start server coroutine");
		free(server);
		return -1;
	}
	return 0;
}

static inline void
od_pooler_main(od_pooler_t *pooler)
{
	od_instance_t *instance = pooler->system->instance;

	/* listen '*' */
	struct addrinfo *hints_ptr = NULL;
	struct addrinfo  hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = IPPROTO_TCP;
	char *host = instance->scheme.host;
	if (strcmp(instance->scheme.host, "*") == 0) {
		hints_ptr = &hints;
		host = NULL;
	}

	/* resolve listen address and port */
	char port[16];
	snprintf(port, sizeof(port), "%d", instance->scheme.port);
	struct addrinfo *ai = NULL;
	int rc;
	rc = machine_getaddrinfo(host, port, hints_ptr, &ai, UINT32_MAX);
	if (rc != 0) {
		od_error(&instance->log, "pooler", "failed to resolve %s:%d",
		          instance->scheme.host,
		          instance->scheme.port);
		return;
	}
	pooler->addr = ai;

	/* listen resolved addresses */
	if (host) {
		od_pooler_server_start(pooler, ai);
		return;
	}
	while (ai) {
		od_pooler_server_start(pooler, ai);
		ai = ai->ai_next;
	}
}

static inline void
od_signalizer(void *arg)
{
	od_pooler_t *pooler = arg;
	od_instance_t *instance = pooler->system->instance;

	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	int rc;
	rc = machine_signal_init(&mask);
	if (rc == -1) {
		od_error(&instance->log, "pooler", "failed to init signal handler");
		return;
	}
	for (;;)
	{
		rc = machine_signal_wait(UINT32_MAX);
		if (rc == -1)
			break;
		switch (rc) {
		case SIGINT:
			od_log(&instance->log, "SIGINT received, shutting down");

			exit(0);
			break;
		}
	}
}

static inline void
od_pooler(void *arg)
{
	od_pooler_t *pooler = arg;
	od_instance_t *instance = pooler->system->instance;

	/* start signal handler coroutine */
	int64_t coroutine_id;
	coroutine_id = machine_coroutine_create(od_signalizer, pooler);
	if (coroutine_id == -1) {
		od_error(&instance->log, "pooler", "failed to start signal handler");
		return;
	}

	/* start router coroutine */
	int rc;
	od_router_t *router;
	router = pooler->system->router;
	rc = od_router_start(router);
	if (rc == -1)
		return;

	/* start periodic coroutine */
	od_periodic_t *periodic;
	periodic = pooler->system->periodic;
	rc = od_periodic_start(periodic);
	if (rc == -1)
		return;

	/* start pooler server */
	od_pooler_main(pooler);
}

int od_pooler_init(od_pooler_t *pooler, od_system_t *system)
{
	od_instance_t *instance = system->instance;

	pooler->machine = -1;
	pooler->system = system;
	pooler->addr = NULL;
	pooler->tls = NULL;

	/* init pooler tls */
	od_scheme_t *scheme = &instance->scheme;
	if (scheme->tls_verify != OD_TDISABLE) {
		pooler->tls = od_tls_frontend(scheme);
		if (pooler->tls == NULL)
			return -1;
	}
	return 0;
}

int od_pooler_start(od_pooler_t *pooler)
{
	od_instance_t *instance = pooler->system->instance;
	pooler->machine = machine_create("pooler", od_pooler, pooler);
	if (pooler->machine == -1) {
		od_error(&instance->log, "pooler", "failed to create pooler thread");
		return -1;
	}
	return 0;
}
