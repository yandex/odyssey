
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
#include <ctype.h>
#include <inttypes.h>
#include <assert.h>

#include <machinarium.h>
#include <kiwi.h>
#include <odyssey.h>

int
od_cancel(od_global_t *global,
          od_config_storage_t *server_config,
          kiwi_key_t *key,
          od_id_t *server_id)
{
	od_instance_t *instance = global->instance;
	od_log(&instance->logger, "cancel", NULL, NULL,
	       "cancel for %s%.*s",
	       server_id->id_prefix,
	       sizeof(server_id->id), server_id->id);
	od_server_t server;
	od_server_init(&server);
	server.global = global;
	od_backend_connect_cancel(&server, server_config, key);
	od_backend_close_connection(&server);
	od_backend_close(&server);
	return 0;
}

static inline int
od_cancel_cmp(od_server_t *server, void *arg)
{
	kiwi_key_t *key = arg;
	return kiwi_key_cmp(&server->key_client, key);
}

int od_cancel_find(od_route_pool_t *route_pool, kiwi_key_t *key,
                   od_router_cancel_t *cancel)
{
	/* match server by client key (forge) */
	od_server_t *server;
	server = od_route_pool_server_foreach(route_pool, OD_SERVER_ACTIVE,
	                                      od_cancel_cmp,
	                                      key);
	if (server == NULL)
		return -1;
	od_route_t *route = server->route;
	cancel->id = server->id;
	cancel->config = od_config_storage_copy(route->config->storage);
	if (cancel->config == NULL)
		return -1;
	cancel->key = server->key;
	return 0;
}
