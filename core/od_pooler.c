
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
#include "od_pooler.h"
#include "od_periodic.h"
#include "od_router.h"

static inline void
od_pooler(void *arg)
{
	odpooler_t *pooler = arg;
	od_t *env = pooler->od;

	/* bind to listen address and port */
	int rc;
	rc = mm_bind(pooler->server, env->scheme.host,
	             env->scheme.port);
	if (rc < 0) {
		od_error(&env->log, "bind %s:%d failed",
		         env->scheme.host,
		         env->scheme.port);
		return;
	}

	/* starting periodic task scheduler fiber */
	rc = mm_create(pooler->env, od_periodic, pooler);
	if (rc < 0) {
		od_error(&env->log, "failed to create periodic fiber");
		return;
	}

	od_log(&env->log, "pooler started at %s:%d",
	       env->scheme.host, env->scheme.port);
	od_log(&env->log, "");

	/* accept loop */
	while (mm_is_online(pooler->env))
	{
		mmio_t client_io;
		rc = mm_accept(pooler->server, env->scheme.backlog, &client_io);
		if (rc < 0) {
			od_error(&env->log, "accept failed");
			continue;
		}
		mm_io_nodelay(client_io, env->scheme.nodelay);
		if (env->scheme.keepalive > 0)
			mm_io_keepalive(client_io, 1, env->scheme.keepalive);
		odclient_t *client = od_clientpool_new(&pooler->client_pool);
		if (client == NULL) {
			od_error(&env->log, "failed to allocate client object");
			mm_close(client_io);
			continue;
		}
		client->id = pooler->client_seq++;
		client->pooler = pooler;
		client->io = client_io;
		rc = mm_create(pooler->env, od_router, client);
		if (rc < 0) {
			od_error(&env->log, "failed to create client fiber");
			mm_close(client_io);
			od_clientpool_unlink(&pooler->client_pool, client);
			continue;
		}
	}
}

int od_pooler_init(odpooler_t *pooler, od_t *od)
{
	pooler->env = mm_new();
	if (pooler->env == NULL)
		return -1;
	pooler->server = mm_io_new(pooler->env);
	if (pooler->server == NULL) {
		mm_free(pooler->env);
		return -1;
	}
	pooler->client_seq = 0;
	pooler->od = od;
	od_routepool_init(&pooler->route_pool);
	od_clientpool_init(&pooler->client_pool);
	return 0;
}

int od_pooler_start(odpooler_t *pooler)
{
	int rc;
	rc = mm_create(pooler->env, od_pooler, pooler);
	if (rc < 0) {
		od_error(&pooler->od->log, "failed to create pooler fiber");
		return -1;
	}
	mm_start(pooler->env);
	return 0;
}
