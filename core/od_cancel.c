
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
#include "od_server.h"
#include "od_server_pool.h"
#include "od_route_id.h"
#include "od_route.h"
#include "od_route_pool.h"
#include "od_client.h"
#include "od_client_pool.h"
#include "od.h"
#include "od_io.h"
#include "od_pooler.h"
#include "od_cancel.h"
#include "od_fe.h"
#include "od_be.h"

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
	mm_io_t io = mm_io_new(pooler->env);
	if (io == NULL)
		return -1;

	/* resolve server address */
	mm_io_t resolver_context = mm_io_new(pooler->env);
	if (resolver_context == NULL) {
		od_error(&pooler->od->log, NULL, "failed to resolve %s:%d",
		         server_scheme->host,
		         server_scheme->port);
		return -1;
	}
	char port[16];
	snprintf(port, sizeof(port), "%d", server_scheme->port);
	struct addrinfo *ai = NULL;
	int rc;
	rc = mm_getaddrinfo(pooler->server,
	                    server_scheme->host, port, NULL, &ai, 0);
	mm_close(resolver_context);
	if (rc < 0) {
		od_error(&pooler->od->log, NULL, "failed to resolve %s:%d",
		         server_scheme->host,
		         server_scheme->port);
		return -1;
	}
	assert(ai != NULL);

	/* connect to server */
	rc = mm_connect(io, ai->ai_addr, 0);
	freeaddrinfo(ai);
	if (rc < 0) {
		od_error(&pooler->od->log, NULL, "(cancel) failed to connect to %s:%d",
		         server_scheme->host,
		         server_scheme->port);
		mm_close(io);
		return -1;
	}
	mm_io_nodelay(io, pooler->od->scheme.nodelay);
	if (pooler->od->scheme.keepalive > 0)
		mm_io_keepalive(io, 1, pooler->od->scheme.keepalive);
	/* send cancel and disconnect */
	so_stream_t stream;
	so_stream_init(&stream);
	rc = so_fewrite_cancel(&stream, key->key_pid, key->key);
	if (rc == -1) {
		mm_close(io);
		so_stream_free(&stream);
		return -1;
	}
	od_write(io, &stream);
	mm_close(io);
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
