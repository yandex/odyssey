
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
#include <signal.h>

#include <machinarium.h>
#include <shapito.h>

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
#include "od_route_pool.h"
#include "od_io.h"

#include "od_router.h"
#include "od_pooler.h"
#include "od_relay.h"
#include "od_frontend.h"
#include "od_backend.h"
#include "od_auth.h"
#include "od_tls.h"
#include "od_cancel.h"

int od_cancel(od_instance_t *instance,
              od_schemeserver_t *server_scheme,
              so_key_t *key,
              uint64_t server_id)
{
	od_server_t server;
	od_server_init(&server);
	server.io = machine_io_create();
	if (server.io == NULL)
		return -1;

	od_log_server(&instance->log, 0, "cancel",
	              "new cancel for S%" PRIu64, server_id);

	/* resolve server address */
	char port[16];
	snprintf(port, sizeof(port), "%d", server_scheme->port);
	struct addrinfo *ai = NULL;
	int rc;
	rc = machine_getaddrinfo(server_scheme->host, port, NULL, &ai, 0);
	if (rc < 0) {
		od_error_server(&instance->log, 0, "cancel", "failed to resolve %s:%d",
		                server_scheme->host,
		                server_scheme->port);
		od_backend_close(&server);
		return -1;
	}
	assert(ai != NULL);

	/* set connection options */
	machine_set_nodelay(server.io, instance->scheme.nodelay);
	if (instance->scheme.keepalive > 0)
		machine_set_keepalive(server.io, 1, instance->scheme.keepalive);
	rc = machine_set_readahead(server.io, instance->scheme.readahead);
	if (rc == -1) {
		od_error_server(&instance->log, 0, "cancel", "failed to set readahead");
		od_backend_close(&server);
		return -1;
	}

	/* connect to server */
	rc = machine_connect(server.io, ai->ai_addr, UINT32_MAX);
	freeaddrinfo(ai);
	if (rc < 0) {
		od_error_server(&instance->log, 0, "cancel",
		                "failed to connect to %s:%d",
		                server_scheme->host,
		                server_scheme->port);
		od_backend_close(&server);
		return -1;
	}

	/* handle tls connection */
	if (server_scheme->tls_verify != OD_TDISABLE) {
		server.tls = od_tls_backend(server_scheme);
		if (server.tls == NULL) {
			od_error_server(&instance->log, 0, "cancel",
			                "failed to create tls context",
			                server_scheme->host,
			                server_scheme->port);
			od_backend_close(&server);
			return -1;
		}
		rc = od_tls_backend_connect(server.io, &instance->log, server_scheme);
		if (rc == -1) {
			od_backend_close(&server);
			return -1;
		}
	}

	/* send cancel and disconnect */
	so_stream_reset(&server.stream);
	rc = so_fewrite_cancel(&server.stream, key->key_pid, key->key);
	if (rc == -1) {
		od_backend_close(&server);
		return -1;
	}
	rc = od_write(server.io, &server.stream);
	if (rc == -1) {
		od_error_server(&instance->log, 0, "cancel", "write error: %s",
		                machine_error(server.io));
	}
	od_backend_close(&server);
	return 0;
}

static inline int
od_cancel_cmp(od_server_t *server, void *arg)
{
	so_key_t *key = arg;
	return so_keycmp(&server->key_client, key);
}

int od_cancel_match(od_instance_t *instance,
                    od_routepool_t *route_pool,
                    so_key_t *key)
{
	/* match server by client key (forge) */
	od_server_t *server;
	server = od_routepool_foreach(route_pool, OD_SACTIVE, od_cancel_cmp, key);
	if (server == NULL)
		return -1;
	od_route_t *route = server->route;
	od_schemeserver_t *server_scheme = route->scheme->server;
	so_key_t cancel_key = server->key;
	return od_cancel(instance, server_scheme, &cancel_key, server->id);
}
