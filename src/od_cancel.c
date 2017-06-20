
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
#include "od_id.h"
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

int od_cancel(od_system_t *system,
              od_schemeserver_t *server_scheme,
              so_key_t *key,
              uint64_t server_id)
{
	od_instance_t *instance = system->instance;
	od_log_server(&instance->log, 0, "cancel",
	              "new cancel for S%" PRIu64, server_id);
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
	so_key_t *key = arg;
	return so_keycmp(&server->key_client, key);
}

int od_cancel_match(od_system_t *system,
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
	return od_cancel(system, server_scheme, &cancel_key, server->id);
}
