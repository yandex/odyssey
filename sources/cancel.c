
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
#include <signal.h>

#include <machinarium.h>
#include <shapito.h>

#include "sources/macro.h"
#include "sources/version.h"
#include "sources/atomic.h"
#include "sources/util.h"
#include "sources/error.h"
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/logger.h"
#include "sources/daemon.h"
#include "sources/scheme.h"
#include "sources/scheme_mgr.h"
#include "sources/config.h"
#include "sources/msg.h"
#include "sources/system.h"
#include "sources/server.h"
#include "sources/server_pool.h"
#include "sources/client.h"
#include "sources/client_pool.h"
#include "sources/route_id.h"
#include "sources/route.h"
#include "sources/route_pool.h"
#include "sources/io.h"
#include "sources/instance.h"
#include "sources/router.h"
#include "sources/pooler.h"
#include "sources/relay.h"
#include "sources/frontend.h"
#include "sources/backend.h"
#include "sources/auth.h"
#include "sources/tls.h"
#include "sources/cancel.h"

int od_cancel(od_system_t *system,
              od_schemestorage_t *server_scheme,
              shapito_key_t *key,
              od_id_t *server_id)
{
	od_instance_t *instance = system->instance;
	od_log(&instance->logger, "cancel", NULL, NULL,
	       "cancel for %s%.*s",
	       server_id->id_prefix,
	       sizeof(server_id->id), server_id->id);
	od_server_t server;
	od_server_init(&server);
	server.system = system;
	od_backend_connect_cancel(&server, server_scheme, key);
	od_backend_close(&server);
	return 0;
}

static inline int
od_cancel_cmp(od_server_t *server, void *arg)
{
	shapito_key_t *key = arg;
	return shapito_key_cmp(&server->key_client, key);
}

int od_cancel_match(od_system_t *system,
                    od_routepool_t *route_pool,
                    shapito_key_t *key)
{
	/* match server by client key (forge) */
	od_server_t *server;
	server = od_routepool_server_foreach(route_pool, OD_SACTIVE,
	                                     od_cancel_cmp,
	                                     key);
	if (server == NULL)
		return -1;
	od_route_t *route = server->route;
	od_schemestorage_t *server_scheme = route->scheme->storage;
	shapito_key_t cancel_key = server->key;
	return od_cancel(system, server_scheme, &cancel_key, &server->id);
}
