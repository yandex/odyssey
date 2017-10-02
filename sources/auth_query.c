
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
#include "sources/error.h"
#include "sources/atomic.h"
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
#include "sources/reset.h"
#include "sources/auth.h"
#include "sources/auth_query.h"

int od_auth_query(od_system_t *system, od_schemeroute_t *scheme,
                  shapito_password_t *password)
{
	od_instance_t *instance = system->instance;

	/* create internal auth client */
	od_client_t *auth_client;
	auth_client = od_client_allocate();
	if (auth_client == NULL)
		return -1;
	auth_client->system = system;

	od_idmgr_generate(&instance->id_mgr, &auth_client->id, "a");

	/* set auth query route db and user */
	shapito_parameters_add(&auth_client->startup.params, "database", 9,
	                       scheme->auth_query_db,
	                       strlen(scheme->auth_query_db) + 1);

	shapito_parameters_add(&auth_client->startup.params, "user", 5,
	                       scheme->auth_query_user,
	                       strlen(scheme->auth_query_user) + 1);

	shapito_parameter_t *param;
	param = (shapito_parameter_t*)auth_client->startup.params.buf.start;
	auth_client->startup.database = param;

	param = shapito_parameter_next(param);
	auth_client->startup.user = param;

	/* route */
	od_routerstatus_t status;
	status = od_route(auth_client);
	if (status != OD_ROK) {
		od_client_free(auth_client);
		return -1;
	}

	/* attach */
	status = od_router_attach(auth_client);
	if (status != OD_ROK) {
		od_unroute(auth_client);
		od_client_free(auth_client);
		return -1;
	}
	od_server_t *server;
	server = auth_client->server;

	od_debug(&instance->logger, "auth_query", NULL, server,
	         "attached to %s%.*s",
	         server->id.id_prefix, sizeof(server->id.id),
	         server->id.id);

	/* connect to server, if necessary */
	int rc;
	if (server->io == NULL) {
		rc = od_backend_connect(server, "auth_query");
		if (rc == -1) {
			od_router_close_and_unroute(auth_client);
			od_client_free(auth_client);
			return -1;
		}
	}

	/* discard last server configuration */
	od_route_t *route;
	route = server->route;
	if (route->scheme->pool_discard) {
		rc = od_reset_discard(server, NULL);
		if (rc == -1) {
			od_router_close_and_unroute(auth_client);
			od_client_free(auth_client);
			return -1;
		}
	}

	/* query */
	rc = od_backend_query(server, &auth_client->stream, "auth_query",
	                      scheme->auth_query,
	                      strlen(scheme->auth_query) + 1);
	if (rc == -1) {
		od_router_close_and_unroute(auth_client);
		od_client_free(auth_client);
		return -1;
	}

	/* parse */
	/* fill password */
	(void)password;

	/* detach and unroute */
	od_router_detach_and_unroute(auth_client);
	od_client_free(auth_client);
	return 0;
}
