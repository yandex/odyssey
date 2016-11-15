
/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <flint.h>
#include <soprano.h>

#include "od_macro.h"
#include "od_list.h"
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
#include "od_pooler.h"
#include "od_router.h"

static inline void
od_pooler(void *arg)
{
	odpooler_t *pooler = arg;
	od_t *env = pooler->od;

	/* bind to listen address and port */
	int rc;
	rc = ft_bind(pooler->server, env->scheme.host,
	             env->scheme.port);
	if (rc < 0) {
		od_error(&env->log, "bind %s:%d failed",
		         env->scheme.host,
		         env->scheme.port);
		return;
	}
	od_log(&env->log, "pooler started at %s:%d",
	       env->scheme.host, env->scheme.port);
	od_log(&env->log, "");

	/* accept loop */
	while (ft_is_online(pooler->env))
	{
		ftio_t client_io;
		rc = ft_accept(pooler->server, &client_io);
		if (rc < 0) {
			od_error(&env->log, "accept failed");
			continue;
		}
		odclient_t *client = od_clientpool_new(&pooler->client_pool);
		if (client == NULL) {
			od_error(&env->log, "failed to allocate client object");
			ft_close(client_io);
			continue;
		}
		client->pooler = pooler;
		client->io = client_io;
		rc = ft_create(pooler->env, od_router, client);
		if (rc < 0) {
			od_error(&env->log, "failed to create client fiber");
			ft_close(client_io);
			od_clientpool_unlink(&pooler->client_pool, client);
			continue;
		}
	}
}

int od_pooler_init(odpooler_t *pooler, od_t *od)
{
	pooler->env = ft_new();
	if (pooler->env == NULL)
		return -1;
	pooler->server = ft_io_new(pooler->env);
	if (pooler->server == NULL) {
		ft_free(pooler->env);
		return -1;
	}
	pooler->od = od;
	od_routepool_init(&pooler->route_pool);
	od_clientpool_init(&pooler->client_pool);
	return 0;
}

int od_pooler_start(odpooler_t *pooler)
{
	int rc;
	rc = ft_create(pooler->env, od_pooler, pooler);
	if (rc < 0) {
		od_error(&pooler->od->log, "failed to create pooler fiber");
		return -1;
	}
	ft_start(pooler->env);
	return 0;
}
