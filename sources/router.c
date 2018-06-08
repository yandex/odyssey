
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
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
#include "sources/config.h"
#include "sources/config_reader.h"
#include "sources/msg.h"
#include "sources/global.h"
#include "sources/server.h"
#include "sources/server_pool.h"
#include "sources/client.h"
#include "sources/client_pool.h"
#include "sources/route_id.h"
#include "sources/route.h"
#include "sources/route_pool.h"
#include "sources/io.h"
#include "sources/instance.h"
#include "sources/router_cancel.h"
#include "sources/router.h"
#include "sources/system.h"
#include "sources/worker.h"
#include "sources/frontend.h"
#include "sources/backend.h"
#include "sources/cancel.h"
#include "sources/cron.h"

typedef struct
{
	od_routerstatus_t status;
	od_client_t *client;
	machine_channel_t *response;
	od_routercancel_t *cancel;
} od_msgrouter_t;

static od_route_t*
od_router_fwd(od_router_t *router, shapito_be_startup_t *startup)
{
	od_instance_t *instance = router->global->instance;

	assert(startup->database != NULL);
	assert(startup->user != NULL);

	/* match latest version of route config */
	od_configroute_t *config;
	config = od_configroute_forward(&instance->config,
	                                shapito_parameter_value(startup->database),
	                                shapito_parameter_value(startup->user));
	if (config == NULL)
		return NULL;

	od_routeid_t id = {
		.database     = shapito_parameter_value(startup->database),
		.database_len = startup->database->value_len,
		.user         = shapito_parameter_value(startup->user),
		.user_len     = startup->user->value_len
	};

	/* force settings required by route */
	if (config->storage_db) {
		id.database = config->storage_db;
		id.database_len = strlen(config->storage_db) + 1;
	}
	if (config->storage_user) {
		id.user = config->storage_user;
		id.user_len = strlen(config->storage_user) + 1;
	}

	/* match or create dynamic route */
	od_route_t *route;
	route = od_routepool_match(&router->route_pool, &id, config);
	if (route)
		return route;
	route = od_routepool_new(&router->route_pool, config, &id);
	if (route == NULL) {
		od_error(&instance->logger, "router", NULL, NULL,
		         "failed to allocate route");
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
	instance = client->global->instance;

	od_router_t *router;
	router = client->global->router;

	od_route_t  *route;
	route = client->route;

	/* get server connection from route idle pool */
	od_server_t *server;
	for (;;)
	{
		server = od_serverpool_next(&route->server_pool, OD_SIDLE);
		if (server)
			goto on_attach;

		/* always start new connection, if pool_size is zero */
		if (route->config->pool_size == 0)
			break;

		/* maybe start new connection */
		if (od_serverpool_total(&route->server_pool) < route->config->pool_size)
			break;

		/* pool_size limit implementation.
		 *
		 * If the limit reached, wait wakeup condition for
		 * pool_timeout milliseconds.
		 *
		 * The condition triggered when a server connection
		 * put into idle state by DETACH events.
		 */
		od_debug(&instance->logger, "router", client, NULL,
		         "route '%s.%s' pool limit reached (%d), waiting",
		          route->config->db_name,
		          route->config->user_name,
		          route->config->pool_size);

		/* enqueue client */
		od_clientpool_set(&route->client_pool, client, OD_CQUEUE);

		uint32_t timeout = route->config->pool_timeout;
		if (timeout == 0)
			timeout = UINT32_MAX;
		int rc;
		rc = machine_condition(timeout);
		if (rc == -1) {
			od_clientpool_set(&route->client_pool, client, OD_CPENDING);
			od_error(&instance->logger, "router", client, NULL,
			         "route '%s.%s' server pool wait timedout, closing",
			         route->config->db_name,
			         route->config->user_name);
			msg_attach->status = OD_RERROR_TIMEDOUT;
			machine_channel_write(msg_attach->response, msg);
			return;
		}
		assert(client->state == OD_CPENDING);

		/* retry */
		od_debug(&instance->logger, "router", client, NULL,
		         "server pool attach retry");
		continue;
	}

	/* create new server object */
	server = od_server_allocate();
	if (server == NULL) {
		msg_attach->status = OD_RERROR;
		machine_channel_write(msg_attach->response, msg);
		return;
	}
	od_idmgr_generate(&instance->id_mgr, &server->id, "s");
	server->global = router->global;
	server->route = route;

on_attach:
	od_serverpool_set(&route->server_pool, server, OD_SACTIVE);
	od_clientpool_set(&route->client_pool, client, OD_CACTIVE);
	client->server = server;
	server->client = client;
	server->idle_time = 0;
	/* assign client session key */
	server->key_client = client->key;
	msg_attach->status = OD_ROK;
	machine_channel_write(msg_attach->response, msg);
}

static inline void
od_router_wakeup(od_router_t *router, od_route_t *route)
{
	od_instance_t *instance;
	instance = router->global->instance;
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
		od_debug(&instance->logger, "router", NULL, NULL,
		         "server released, waking up");
	}
}

static inline void
od_router(void *arg)
{
	od_router_t *router = arg;
	od_instance_t *instance = router->global->instance;

	for (;;)
	{
		machine_msg_t *msg;
		msg = machine_channel_read(router->channel, UINT32_MAX);
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
			if (instance->config.client_max_set) {
				if (router->clients >= instance->config.client_max) {
					od_log(&instance->logger, "router", NULL, NULL,
					       "router: global client_max limit reached (%d)",
					       instance->config.client_max);
					msg_route->status = OD_RERROR_LIMIT;
					machine_channel_write(msg_route->response, msg);
					break;
				}
			}

			/* match route */
			od_route_t *route;
			route = od_router_fwd(router, &msg_route->client->startup);
			if (route == NULL) {
				msg_route->status = OD_RERROR_NOT_FOUND;
				machine_channel_write(msg_route->response, msg);
				break;
			}

			/* ensure route client_max limit */
			if (route->config->client_max_set) {
				int client_total;
				client_total = od_clientpool_total(&route->client_pool);
				if (client_total >= route->config->client_max) {
					od_log(&instance->logger, "router", NULL, NULL,
					       "route '%s.%s' client_max limit reached (%d)",
					       route->config->db_name,
					       route->config->user_name,
					       route->config->client_max);
					msg_route->status = OD_RERROR_LIMIT;
					machine_channel_write(msg_route->response, msg);
					break;
				}
			}

			/* add client to route client pool */
			od_clientpool_set(&route->client_pool, msg_route->client, OD_CPENDING);
			router->clients++;

			msg_route->client->config = route->config;
			msg_route->client->route = route;
			msg_route->status = OD_ROK;
			machine_channel_write(msg_route->response, msg);
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

			msg_unroute->status = OD_ROK;
			machine_channel_write(msg_unroute->response, msg);
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
				machine_channel_write(msg_attach->response, msg);
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
			server->client = NULL;
			server->last_client_id = client->id;
			od_serverpool_set(&route->server_pool, server, OD_SIDLE);
			od_clientpool_set(&route->client_pool, client, OD_CPENDING);

			/* wakeup attachers */
			od_router_wakeup(router, route);

			msg_detach->status = OD_ROK;
			machine_channel_write(msg_detach->response, msg);
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
			server->client = NULL;
			od_serverpool_set(&route->server_pool, server, OD_SIDLE);

			client->server = NULL;
			client->route = NULL;
			od_clientpool_set(&route->client_pool, client, OD_CUNDEF);
			assert(router->clients > 0);
			router->clients--;

			/* wakeup attachers */
			od_router_wakeup(router, route);

			msg_detach->status = OD_ROK;
			machine_channel_write(msg_detach->response, msg);
			break;
		}

		case OD_MROUTER_CLOSE:
		{
			/* detach closed server connection */
			od_msgrouter_t *msg_detach;
			msg_detach = machine_msg_get_data(msg);

			od_client_t *client = msg_detach->client;
			od_route_t *route = client->route;
			od_server_t *server = client->server;

			client->server = NULL;
			od_clientpool_set(&route->client_pool, client, OD_CPENDING);

			od_serverpool_set(&route->server_pool, server, OD_SUNDEF);
			server->last_client_id = client->id;
			server->client = NULL;
			server->route  = NULL;

			assert(server->io == NULL);
			od_backend_close(server);

			msg_detach->status = OD_ROK;
			machine_channel_write(msg_detach->response, msg);
			break;
		}

		case OD_MROUTER_CLOSE_AND_UNROUTE:
		{
			/* detach closed server connection and unroute client */
			od_msgrouter_t *msg_close;
			msg_close = machine_msg_get_data(msg);

			od_client_t *client = msg_close->client;
			od_route_t *route = client->route;
			od_server_t *server = client->server;
			od_serverpool_set(&route->server_pool, server, OD_SUNDEF);
			server->client = NULL;
			server->route  = NULL;

			/* remove client from route client pool */
			client->server = NULL;
			client->route  = NULL;
			od_clientpool_set(&route->client_pool, client, OD_CUNDEF);
			assert(router->clients > 0);
			router->clients--;

			assert(server->io == NULL);
			od_backend_close(server);

			msg_close->status = OD_ROK;
			machine_channel_write(msg_close->response, msg);
			break;
		}

		case OD_MROUTER_CANCEL:
		{
			/* match server key and config by client key */
			od_msgrouter_t *msg_cancel;
			msg_cancel = machine_msg_get_data(msg);
			int rc;
			rc = od_cancel_find(&router->route_pool,
			                    &msg_cancel->client->startup.key,
			                     msg_cancel->cancel);
			if (rc == -1)
				msg_cancel->status = OD_RERROR;
			else
				msg_cancel->status = OD_ROK;
			machine_channel_write(msg_cancel->response, msg);
			break;
		}
		default:
			assert(0);
			break;
		}
	}
}

void od_router_init(od_router_t *router, od_global_t *global)
{
	od_routepool_init(&router->route_pool);
	router->global  = global;
	router->clients = 0;
	router->channel = NULL;
}

int od_router_start(od_router_t *router)
{
	od_instance_t *instance = router->global->instance;

	router->channel = machine_channel_create(instance->is_shared);
	if (router->channel == NULL) {
		od_error(&instance->logger, "router", NULL, NULL,
		         "failed to create router channel");
		return -1;
	}
	int64_t coroutine_id;
	coroutine_id = machine_coroutine_create(od_router, router);
	if (coroutine_id == -1) {
		od_error(&instance->logger, "router", NULL, NULL,
		         "failed to start router");
		return -1;
	}
	return 0;
}

static od_routerstatus_t
od_router_do(od_client_t *client, od_msg_t msg_type, od_routercancel_t *cancel)
{
	od_router_t *router = client->global->router;
	od_instance_t *instance = router->global->instance;

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
	msg_route->cancel = cancel;

	/* create response channel */
	machine_channel_t *response;
	response = machine_channel_create(instance->is_shared);
	if (response == NULL) {
		machine_msg_free(msg);
		return OD_RERROR;
	}
	msg_route->response = response;
	machine_channel_write(router->channel, msg);

	/* wait for reply */
	msg = machine_channel_read(response, UINT32_MAX);
	if (msg == NULL) {
		/* todo:  */
		abort();
		machine_channel_free(response);
		return OD_RERROR;
	}
	msg_route = machine_msg_get_data(msg);
	od_routerstatus_t status;
	status = msg_route->status;
	machine_channel_free(response);
	machine_msg_free(msg);
	return status;
}

od_routerstatus_t
od_route(od_client_t *client)
{
	return od_router_do(client, OD_MROUTER_ROUTE, NULL);
}

od_routerstatus_t
od_unroute(od_client_t *client)
{
	return od_router_do(client, OD_MROUTER_UNROUTE, NULL);
}

od_routerstatus_t
od_router_attach(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;
	od_routerstatus_t status;
	status = od_router_do(client, OD_MROUTER_ATTACH, NULL);
	/* attach server io to clients machine context */
	od_server_t *server = client->server;
	if (instance->is_shared) {
		if (server && server->io)
			machine_io_attach(server->io);
	}
	return status;
}

od_routerstatus_t
od_router_detach(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;
	od_server_t *server = client->server;
	if (instance->is_shared)
		machine_io_detach(server->io);
	return od_router_do(client, OD_MROUTER_DETACH, NULL);
}

od_routerstatus_t
od_router_detach_and_unroute(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;
	od_server_t *server = client->server;
	if (instance->is_shared)
		machine_io_detach(server->io);
	return od_router_do(client, OD_MROUTER_DETACH_AND_UNROUTE, NULL);
}

od_routerstatus_t
od_router_close(od_client_t *client)
{
	od_server_t *server = client->server;
	od_backend_close_connection(server);
	return od_router_do(client, OD_MROUTER_CLOSE, NULL);
}

od_routerstatus_t
od_router_close_and_unroute(od_client_t *client)
{
	od_server_t *server = client->server;
	od_backend_close_connection(server);
	return od_router_do(client, OD_MROUTER_CLOSE_AND_UNROUTE, NULL);
}

od_routerstatus_t
od_router_cancel(od_client_t *client, od_routercancel_t *cancel)
{
	return od_router_do(client, OD_MROUTER_CANCEL, cancel);
}
