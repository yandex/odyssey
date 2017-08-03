
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
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <machinarium.h>
#include <shapito.h>

#include "sources/macro.h"
#include "sources/version.h"
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/log_file.h"
#include "sources/log_system.h"
#include "sources/logger.h"
#include "sources/daemon.h"
#include "sources/scheme.h"
#include "sources/scheme_mgr.h"
#include "sources/config.h"
#include "sources/msg.h"
#include "sources/system.h"
#include "sources/instance.h"
#include "sources/server.h"
#include "sources/server_pool.h"
#include "sources/client.h"
#include "sources/client_pool.h"
#include "sources/route_id.h"
#include "sources/route.h"
#include "sources/route_pool.h"
#include "sources/io.h"
#include "sources/router.h"
#include "sources/console.h"
#include "sources/relay.h"
#include "sources/relay_pool.h"
#include "sources/pooler.h"
#include "sources/periodic.h"
#include "sources/tls.h"

static inline void
od_pooler_server(void *arg)
{
	od_poolerserver_t *server = arg;
	od_instance_t *instance = server->system->instance;
	od_relaypool_t *relay_pool = server->system->relay_pool;

	/* create server tls */
	if (instance->scheme.tls_mode != OD_TLS_DISABLE) {
		server->tls = od_tls_frontend(&instance->scheme);
		if (server->tls == NULL) {
			od_error(&instance->logger, "server", "failed to create tls handler");
			return;
		}
	}

	/* create server io */
	machine_io_t *server_io;
	server_io = machine_io_create();
	if (server_io == NULL) {
		od_error(&instance->logger, "server", "failed to create pooler io");
		return;
	}

	char addr_name[128];
	od_getaddrname(server->addr, addr_name, sizeof(addr_name));

	/* bind to listen address and port */
	int rc;
	rc = machine_bind(server_io, server->addr->ai_addr);
	if (rc == -1) {
		od_error(&instance->logger, "server", "bind to %s failed: %s",
		         addr_name,
		         machine_error(server_io));
		machine_close(server_io);
		machine_io_free(server_io);
		return;
	}

	od_log(&instance->logger, "listening on %s", addr_name);

	/* main loop */
	for (;;)
	{
		machine_io_t *client_io;
		rc = machine_accept(server_io, &client_io,
		                    instance->scheme.backlog, UINT32_MAX);
		if (rc == -1) {
			od_error(&instance->logger, "server", "accept failed: %s",
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
			od_error(&instance->logger, "server", "failed to set client readahead: %s",
			         machine_error(client_io));
			machine_close(client_io);
			machine_io_free(client_io);
			continue;
		}

		/* detach io from pooler event loop */
		rc = machine_io_detach(client_io);
		if (rc == -1) {
			od_error(&instance->logger, "server", "failed to transfer client io: %s",
			         machine_error(client_io));
			machine_close(client_io);
			machine_io_free(client_io);
			continue;
		}

		/* allocate new client */
		od_client_t *client = od_client_allocate();
		if (client == NULL) {
			od_error(&instance->logger, "server", "failed to allocate client object");
			machine_close(client_io);
			machine_io_free(client_io);
			continue;
		}
		od_idmgr_generate(&instance->id_mgr, &client->id, "c");
		client->io = client_io;
		client->tls = server->tls;

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
		od_error(&instance->logger, "pooler", "failed to allocate pooler server object");
		return -1;
	}
	server->addr = addr;
	server->system = pooler->system;
	int64_t coroutine_id;
	coroutine_id = machine_coroutine_create(od_pooler_server, server);
	if (coroutine_id == -1) {
		od_error(&instance->logger, "pooler", "failed to start server coroutine");
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
		od_error(&instance->logger, "pooler", "failed to resolve %s:%d",
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
od_pooler_config_import(od_pooler_t *pooler)
{
	od_instance_t *instance = pooler->system->instance;

	od_log(&instance->logger, "(config) importing changes from '%s'",
	       instance->config_file);

	od_scheme_t scheme;
	od_scheme_init(&scheme);
	uint64_t scheme_version;
	scheme_version = od_schememgr_version_next(&instance->scheme_mgr);
	int rc;
	rc = od_config_load(&scheme, &instance->logger, instance->config_file,
	                    scheme_version);
	if (rc == -1) {
		od_scheme_free(&scheme);
		return;
	}
	rc = od_scheme_validate(&scheme, &instance->logger);
	if (rc == -1) {
		od_scheme_free(&scheme);
		return;
	}

	/* Merge configuration changes.
	 *
	 * Add new routes or obsolete previous ones which are updated or not
	 * present in new config file.
	*/
	int has_updates;
	has_updates = od_scheme_merge(&instance->scheme, &instance->logger, &scheme);

	/* free unused settings */
	od_scheme_free(&scheme);

	if (has_updates && instance->scheme.log_config)
		od_scheme_print(&instance->scheme, &instance->logger, 1);
}

static inline void
od_pooler_signal_handler(void *arg)
{
	od_pooler_t *pooler = arg;
	od_instance_t *instance = pooler->system->instance;

	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGHUP);
	int rc;
	rc = machine_signal_init(&mask);
	if (rc == -1) {
		od_error(&instance->logger, "pooler", "failed to init signal handler");
		return;
	}
	for (;;)
	{
		rc = machine_signal_wait(UINT32_MAX);
		if (rc == -1)
			break;
		switch (rc) {
		case SIGINT:
			od_log(&instance->logger, "SIGINT received, shutting down");

			exit(0);
			break;
		case SIGHUP:
			od_log(&instance->logger, "SIGHUP received");
			od_pooler_config_import(pooler);
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
	coroutine_id = machine_coroutine_create(od_pooler_signal_handler, pooler);
	if (coroutine_id == -1) {
		od_error(&instance->logger, "pooler", "failed to start signal handler");
		return;
	}

	/* start router coroutine */
	int rc;
	od_router_t *router;
	router = pooler->system->router;
	rc = od_router_start(router);
	if (rc == -1)
		return;

	/* start console coroutine */
	od_console_t *console;
	console = pooler->system->console;
	rc = od_console_start(console);
	if (rc == -1)
		return;

	/* start periodic coroutine */
	od_periodic_t *periodic;
	periodic = pooler->system->periodic;
	rc = od_periodic_start(periodic);
	if (rc == -1)
		return;

	/* start pooler servers */
	od_pooler_main(pooler);
}

int od_pooler_init(od_pooler_t *pooler, od_system_t *system)
{
	pooler->machine = -1;
	pooler->system = system;
	pooler->addr = NULL;
	return 0;
}

int od_pooler_start(od_pooler_t *pooler)
{
	od_instance_t *instance = pooler->system->instance;
	pooler->machine = machine_create("pooler", od_pooler, pooler);
	if (pooler->machine == -1) {
		od_error(&instance->logger, "pooler", "failed to create pooler thread");
		return -1;
	}
	return 0;
}
