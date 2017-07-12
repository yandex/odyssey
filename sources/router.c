
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
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/syslog.h"
#include "sources/log.h"
#include "sources/daemon.h"
#include "sources/scheme.h"
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
#include "sources/pooler.h"
#include "sources/relay.h"
#include "sources/frontend.h"
#include "sources/backend.h"
#include "sources/cancel.h"
#include "sources/periodic.h"

typedef struct
{
	od_routerstatus_t status;
	od_client_t *client;
	machine_queue_t *response;
} od_msgrouter_t;

static od_route_t*
od_router_fwd(od_router_t *router, shapito_be_startup_t *startup)
{
	od_instance_t *instance = router->system->instance;

	assert(startup->database != NULL);
	assert(startup->user != NULL);

	/* match requested db and user scheme */
	od_schemedb_t *db_scheme =
		od_schemedb_match(&instance->scheme,
		                  shapito_parameter_value(startup->database));
	if (db_scheme == NULL) {
		db_scheme = instance->scheme.db_default;
		if (db_scheme == NULL)
			return NULL;
	}
	od_schemeuser_t *user_scheme =
		od_schemeuser_match(db_scheme,
		                    shapito_parameter_value(startup->user));
	if (user_scheme == NULL) {
		user_scheme = db_scheme->user_default;
		if (user_scheme == NULL)
			return NULL;
	}

	od_routeid_t id = {
		.database     = shapito_parameter_value(startup->database),
		.database_len = startup->database->value_len,
		.user         = shapito_parameter_value(startup->user),
		.user_len     = startup->user->value_len
	};

	/* force settings required by route */
	if (user_scheme->storage_db) {
		id.database = user_scheme->storage_db;
		id.database_len = strlen(user_scheme->storage_db) + 1;
	}
	if (user_scheme->storage_user) {
		id.user = user_scheme->storage_user;
		id.user_len = strlen(user_scheme->storage_user) + 1;
	}

	/* match or create dynamic route */
	od_route_t *route;
	route = od_routepool_match(&router->route_pool, &id);
	if (route)
		return route;
	route = od_routepool_new(&router->route_pool, user_scheme, &id);
	if (route == NULL) {
		od_error(&instance->log, "router", "failed to allocate route");
		return NULL;
	}
	return route;
}

static inline void
od_router_attacher(void *arg)
{
	machine_msg_t *msg;
	msg = arg;

	od_msgrouter_t *msg_attach;
	msg_attach = machine_msg_get_data(arg);

	od_client_t *client;
	client = msg_attach->client;

	od_instance_t *instance;
	instance = client->system->instance;

	od_router_t *router;
	router = client->system->router;

	od_route_t  *route;
	route = client->route;

	/* get server connection from route idle pool */
	od_server_t *server;
	for (;;)
	{
		server = od_serverpool_next(&route->server_pool, OD_SIDLE);
		if (server)
			goto on_attach;

		/* maybe start new connection */
		if (od_serverpool_total(&route->server_pool) < route->scheme->pool_size)
			break;

		/* pool_size limit implementation.
		 *
		 * If the limit reached, wait wakeup condition for
		 * pool_timeout milliseconds.
		 *
		 * The condition triggered when a server connection
		 * put into idle state by DETACH events.
		 */
		od_debug_client(&instance->log, &client->id, "router",
		                "route '%s.%s' pool limit reached (%d), waiting",
		                route->scheme->db->name,
		                route->scheme->user,
		                route->scheme->pool_size);

		/* enqueue client */
		od_clientpool_set(&route->client_pool, client, OD_CQUEUE);

		int rc;
		rc = machine_condition(route->scheme->pool_timeout);
		if (rc == -1) {
			od_clientpool_set(&route->client_pool, client, OD_CPENDING);
			od_debug_client(&instance->log, &client->id, "router",
			                "server pool wait timedout, closing");
			msg_attach->status = OD_RERROR_TIMEDOUT;
			machine_queue_put(msg_attach->response, msg);
			return;
		}
		assert(client->state == OD_CPENDING);

		/* retry */
		od_debug_client(&instance->log, &client->id, "router",
		                "server pool attach retry");
		continue;
	}

	/* create new server object */
	server = od_server_allocate();
	if (server == NULL) {
		msg_attach->status = OD_RERROR;
		machine_queue_put(msg_attach->response, msg);
		return;
	}
	od_idmgr_generate(&instance->id_mgr, &server->id);
	server->system = router->system;
	server->route = route;

on_attach:
	od_serverpool_set(&route->server_pool, server, OD_SACTIVE);
	od_clientpool_set(&route->client_pool, client, OD_CACTIVE);
	client->server = server;
	server->idle_time = 0;
	/* assign client session key */
	server->key_client = client->key;
	msg_attach->status = OD_ROK;
	machine_queue_put(msg_attach->response, msg);
}

static inline void
od_router_wakeup(od_router_t *router, od_route_t *route)
{
	od_instance_t *instance;
	instance = router->system->instance;
	/* wake up first client waiting for route
	 * server connection */
	if (route->client_pool.count_queue > 0) {
		od_client_t *waiter;
		waiter = od_clientpool_next(&route->client_pool, OD_CQUEUE);
		int rc;
		rc = machine_signal(waiter->coroutine_attacher_id);
		assert(rc == 0);
		(void)rc;
		od_clientpool_set(&route->client_pool, waiter, OD_CPENDING);
		od_debug_client(&instance->log, &waiter->id, "router",
		                "server released, waking up");
	}
}

static inline void
od_router(void *arg)
{
	od_router_t *router = arg;
	od_instance_t *instance = router->system->instance;

	for (;;)
	{
		machine_msg_t *msg;
		msg = machine_queue_get(router->queue, UINT32_MAX);
		if (msg == NULL)
			break;

		od_msg_t msg_type;
		msg_type = machine_msg_get_type(msg);
		switch (msg_type) {
		case OD_MROUTER_ROUTE:
		{
			/* attach client to route */
			od_msgrouter_t *msg_route;
			msg_route = machine_msg_get_data(msg);

			/* ensure global client_max limit */
			if (instance->scheme.client_max_set) {
				if (router->clients >= instance->scheme.client_max) {
					od_log(&instance->log,
					       "router: global client_max limit reached (%d)",
					       instance->scheme.client_max);
					msg_route->status = OD_RERROR_LIMIT;
					machine_queue_put(msg_route->response, msg);
					break;
				}
			}

			/* match route */
			od_route_t *route;
			route = od_router_fwd(router, &msg_route->client->startup);
			if (route == NULL) {
				msg_route->status = OD_RERROR_NOT_FOUND;
				machine_queue_put(msg_route->response, msg);
				break;
			}

			/* ensure route client_max limit */
			if (route->scheme->client_max_set) {
				int client_total;
				client_total = od_clientpool_total(&route->client_pool);
				if (client_total >= route->scheme->client_max) {
					od_log(&instance->log,
					       "router: route '%s.%s' client_max limit reached (%d)",
					       route->scheme->db->name,
					       route->scheme->user,
					       route->scheme->client_max);
					msg_route->status = OD_RERROR_LIMIT;
					machine_queue_put(msg_route->response, msg);
					break;
				}
			}

			/* add client to route client pool */
			od_clientpool_set(&route->client_pool, msg_route->client, OD_CPENDING);
			router->clients++;

			msg_route->client->scheme = route->scheme;
			msg_route->client->route = route;
			msg_route->status = OD_ROK;
			machine_queue_put(msg_route->response, msg);
			break;
		}

		case OD_MROUTER_UNROUTE:
		{
			/* detach client from route */
			od_msgrouter_t *msg_unroute;
			msg_unroute = machine_msg_get_data(msg);

			od_client_t *client = msg_unroute->client;
			od_route_t *route = client->route;
			client->route = NULL;
			assert(client->server == NULL);
			od_clientpool_set(&route->client_pool, client, OD_CUNDEF);

			assert(router->clients > 0);
			router->clients--;

			/* maybe remove empty route */
			od_routepool_gc_route(&router->route_pool, route);

			msg_unroute->status = OD_ROK;
			machine_queue_put(msg_unroute->response, msg);
			break;
		}

		case OD_MROUTER_ATTACH:
		{
			/* get client server from route server pool */
			od_msgrouter_t *msg_attach;
			msg_attach = machine_msg_get_data(msg);

			od_client_t *client;
			client = msg_attach->client;

			int64_t coroutine_id;
			coroutine_id = machine_coroutine_create(od_router_attacher, msg);
			if (coroutine_id == -1) {
				msg_attach->status = OD_RERROR;
				machine_queue_put(msg_attach->response, msg);
				break;
			}
			client->coroutine_attacher_id = coroutine_id;
			break;
		}

		case OD_MROUTER_DETACH:
		{
			/* push client server back to route server pool */
			od_msgrouter_t *msg_detach;
			msg_detach = machine_msg_get_data(msg);

			od_client_t *client = msg_detach->client;
			od_route_t *route = client->route;
			od_server_t *server = client->server;

			client->server = NULL;
			server->last_client_id = client->id;
			od_serverpool_set(&route->server_pool, server, OD_SIDLE);
			od_clientpool_set(&route->client_pool, client, OD_CPENDING);

			/* wakeup attachers */
			od_router_wakeup(router, route);

			msg_detach->status = OD_ROK;
			machine_queue_put(msg_detach->response, msg);
			break;
		}

		case OD_MROUTER_DETACH_AND_UNROUTE:
		{
			/* push client server back to route server pool,
			 * unroute client */
			od_msgrouter_t *msg_detach;
			msg_detach = machine_msg_get_data(msg);

			od_client_t *client = msg_detach->client;
			od_route_t *route = client->route;
			od_server_t *server = client->server;

			server->last_client_id = client->id;
			client->server = NULL;
			client->route = NULL;
			od_serverpool_set(&route->server_pool, server, OD_SIDLE);
			od_clientpool_set(&route->client_pool, client, OD_CUNDEF);

			/* wakeup attachers */
			od_router_wakeup(router, route);

			msg_detach->status = OD_ROK;
			machine_queue_put(msg_detach->response, msg);
			break;
		}

		case OD_MROUTER_CLOSE_AND_UNROUTE:
		{
			/* detach and close server connection,
			 * unroute client */
			od_msgrouter_t *msg_close;
			msg_close = machine_msg_get_data(msg);

			od_client_t *client = msg_close->client;
			od_route_t *route = client->route;
			od_server_t *server = client->server;
			od_serverpool_set(&route->server_pool, server, OD_SUNDEF);
			server->route = NULL;

			/* remove client from route client pool */
			od_clientpool_set(&route->client_pool, client, OD_CUNDEF);
			client->server = NULL;
			client->route = NULL;

			machine_io_attach(server->io);
			od_backend_terminate(server);
			od_backend_close(server);

			msg_close->status = OD_ROK;
			machine_queue_put(msg_close->response, msg);
			break;
		}

		case OD_MROUTER_CANCEL:
		{
			/* match server by client key and initiate
			 * cancel request connection */
			od_msgrouter_t *msg_cancel;
			msg_cancel = machine_msg_get_data(msg);
			int rc;
			rc = od_cancel_match(router->system,
			                     &router->route_pool,
			                     &msg_cancel->client->startup.key);
			if (rc == -1)
				msg_cancel->status = OD_RERROR;
			else
				msg_cancel->status = OD_ROK;
			machine_queue_put(msg_cancel->response, msg);
			break;
		}
		default:
			assert(0);
			break;
		}
	}
}

int od_router_init(od_router_t *router, od_system_t *system)
{
	od_instance_t *instance = system->instance;
	od_routepool_init(&router->route_pool);
	router->system = system;
	router->clients = 0;
	router->queue = machine_queue_create();
	if (router->queue == NULL) {
		od_error(&instance->log, "router", "failed to create router queue");
		return -1;
	}
	return 0;
}

int od_router_start(od_router_t *router)
{
	od_instance_t *instance = router->system->instance;
	int64_t coroutine_id;
	coroutine_id = machine_coroutine_create(od_router, router);
	if (coroutine_id == -1) {
		od_error(&instance->log, "router", "failed to start router");
		return -1;
	}
	return 0;
}

static od_routerstatus_t
od_router_do(od_client_t *client, od_msg_t msg_type, int wait_for_response)
{
	od_router_t *router = client->system->router;

	/* send request to router */
	machine_msg_t *msg;
	msg = machine_msg_create(msg_type, sizeof(od_msgrouter_t));
	if (msg == NULL)
		return OD_RERROR;
	od_msgrouter_t *msg_route;
	msg_route = machine_msg_get_data(msg);
	msg_route->status = OD_RERROR;
	msg_route->client = client;
	msg_route->response = NULL;

	/* create response queue */
	machine_queue_t *response;
	if (wait_for_response) {
		response = machine_queue_create();
		if (response == NULL) {
			machine_msg_free(msg);
			return OD_RERROR;
		}
		msg_route->response = response;
	}
	machine_queue_put(router->queue, msg);

	if (! wait_for_response)
		return OD_ROK;

	/* wait for reply */
	msg = machine_queue_get(response, UINT32_MAX);
	if (msg == NULL) {
		/* todo:  */
		abort();
		machine_queue_free(response);
		return OD_RERROR;
	}
	msg_route = machine_msg_get_data(msg);
	od_routerstatus_t status;
	status = msg_route->status;
	machine_queue_free(response);
	machine_msg_free(msg);
	return status;
}

od_routerstatus_t
od_route(od_client_t *client)
{
	return od_router_do(client, OD_MROUTER_ROUTE, 1);
}

od_routerstatus_t
od_unroute(od_client_t *client)
{
	return od_router_do(client, OD_MROUTER_UNROUTE, 1);
}

od_routerstatus_t
od_router_attach(od_client_t *client)
{
	od_routerstatus_t status;
	status = od_router_do(client, OD_MROUTER_ATTACH, 1);
	od_server_t *server = client->server;
	if (server && server->io) {
		/* attach server io to clients machine context */
		if (server->io)
			machine_io_attach(server->io);
	}
	return status;
}

od_routerstatus_t
od_router_detach(od_client_t *client)
{
	/* detach server io from clients machine context */
	machine_io_detach(client->server->io);
	return od_router_do(client, OD_MROUTER_DETACH, 1);
}

od_routerstatus_t
od_router_detach_and_unroute(od_client_t *client)
{
	/* detach server io from clients machine context */
	machine_io_detach(client->server->io);
	return od_router_do(client, OD_MROUTER_DETACH_AND_UNROUTE, 1);
}

od_routerstatus_t
od_router_close_and_unroute(od_client_t *client)
{
	/* detach server io from clients machine context */
	machine_io_detach(client->server->io);
	return od_router_do(client, OD_MROUTER_CLOSE_AND_UNROUTE, 1);
}

od_routerstatus_t
od_router_cancel(od_client_t *client)
{
	return od_router_do(client, OD_MROUTER_CANCEL, 1);
}
