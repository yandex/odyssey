
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

#include <machinarium.h>
#include <soprano.h>

#include "od_macro.h"
#include "od_list.h"
#include "od_pid.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"
#include "od_stat.h"
#include "od_server.h"
#include "od_server_pool.h"
#include "od_client.h"
#include "od_client_list.h"
#include "od_client_pool.h"
#include "od_route_id.h"
#include "od_route.h"
#include "od_route_pool.h"
#include "od.h"
#include "od_io.h"
#include "od_pooler.h"
#include "od_cancel.h"
#include "od_fe.h"
#include "od_be.h"
#include "od_tls.h"

static inline int
od_cancel_cmp(od_server_t *server, void *arg)
{
	so_key_t *key = arg;
	return so_keycmp(&server->key_client, key);
}

int od_cancel_of(od_pooler_t *pooler,
                 od_schemeserver_t *server_scheme,
                 so_key_t *key)
{
	machine_io_t io = machine_create_io(pooler->env);
	if (io == NULL)
		return -1;

	/* resolve server address */
	machine_io_t resolver_context = machine_create_io(pooler->env);
	if (resolver_context == NULL) {
		od_error(&pooler->od->log, NULL,
		         "failed to resolve %s:%d",
		         server_scheme->host,
		         server_scheme->port);
		return -1;
	}
	char port[16];
	snprintf(port, sizeof(port), "%d", server_scheme->port);
	struct addrinfo *ai = NULL;
	int rc;
	rc = machine_getaddrinfo(pooler->server,
	                         server_scheme->host, port, NULL, &ai, 0);
	machine_close(resolver_context);
	machine_free_io(resolver_context);
	if (rc < 0) {
		od_error(&pooler->od->log, NULL,
		         "failed to resolve %s:%d",
		         server_scheme->host,
		         server_scheme->port);
		return -1;
	}
	assert(ai != NULL);

	machine_set_nodelay(io, pooler->od->scheme.nodelay);
	if (pooler->od->scheme.keepalive > 0)
		machine_set_keepalive(io, 1, pooler->od->scheme.keepalive);

	/* connect to server */
	rc = machine_connect(io, ai->ai_addr, 0);
	freeaddrinfo(ai);
	if (rc < 0) {
		od_error(&pooler->od->log, NULL,
		         "(cancel) failed to connect to %s:%d",
		         server_scheme->host,
		         server_scheme->port);
		machine_close(io);
		machine_free_io(io);
		return -1;
	}
	rc = machine_set_readahead(io, pooler->od->scheme.readahead);
	if (rc == -1) {
		od_error(&pooler->od->log, NULL, "(cancel) failed to set readahead");
		return -1;
	}

	so_stream_t stream;
	so_stream_init(&stream);

	/* handle tls connection */
	machine_tls_t tls = NULL;
	if (server_scheme->tls_verify != OD_TDISABLE) {
		tls = od_tlsbe(pooler->env, server_scheme);
		if (tls == NULL) {
			od_error(&pooler->od->log, NULL,
			         "(cancel) failed to create tls context",
			         server_scheme->host,
			         server_scheme->port);
			machine_close(io);
			machine_free_io(io);
			return -1;
		}
		rc = od_tlsbe_connect(pooler->env, io, tls,
		                      &stream,
		                      &pooler->od->log, "(cancel)",
		                      server_scheme);
		if (rc == -1) {
			machine_close(io);
			machine_free_io(io);
			machine_free_tls(tls);
			so_stream_free(&stream);
			return -1;
		}
	}

	/* send cancel and disconnect */
	rc = so_fewrite_cancel(&stream, key->key_pid, key->key);
	if (rc == -1) {
		machine_close(io);
		machine_free_io(io);
		if (tls)
			machine_free_tls(tls);
		so_stream_free(&stream);
		return -1;
	}
	rc = od_write(io, &stream);
	if (rc == -1) {
		od_error(&pooler->od->log, io, "(cancel): write error: %s",
		         machine_error(io));
	}
	machine_close(io);
	machine_free_io(io);
	if (tls)
		machine_free_tls(tls);
	so_stream_free(&stream);
	return 0;
}

int od_cancel(od_pooler_t *pooler, so_key_t *key)
{
	/* match server by client key (forge) */
	od_server_t *server;
	server = od_routepool_foreach(&pooler->route_pool, OD_SACTIVE,
	                              od_cancel_cmp, key);
	if (server == NULL)
		return -1;
	od_route_t *route = server->route;
	od_schemeserver_t *server_scheme = route->scheme->server;
	so_key_t cancel_key = server->key;
	return od_cancel_of(pooler, server_scheme, &cancel_key);
}
