
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

#include "od_macro.h"
#include "od_list.h"
#include "od_log.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"
#include "od_server.h"
#include "od_server_pool.h"
#include "od_client.h"
#include "od_client_pool.h"
#include "od.h"
#include "od_pooler.h"

static inline void
od_pooler(void *arg)
{
	odpooler_t *p = arg;
	od_t *env = p->od;

	/* bind to listen address and port */
	int rc;
	rc = ft_bind(p->server, env->scheme.host,
	             env->scheme.port);
	if (rc < 0) {
		od_error(&env->log, "bind %s:%d failed",
		         env->scheme.host,
		         env->scheme.port);
		return;
	}

	/* accept loop */
	while (ft_is_online(p->env))
	{
		ftio_t client_io;
		rc = ft_accept(p->server, &client_io);
		if (rc < 0) {
			od_error(&env->log, "accept failed");
			continue;
		}
		odclient_t *client = od_clientpool_new(&p->client_pool);
		if (client == NULL) {
			od_error(&env->log, "failed to allocate client object");
			ft_close(client_io);
			continue;
		}
		client->io = client_io;
		rc = ft_create(p->env, NULL, client);
		if (rc < 0) {
			od_error(&env->log, "failed to create client fiber");
			ft_close(client_io);
			od_clientpool_unlink(&p->client_pool, client);
			continue;
		}
	}
}

int od_pooler_init(odpooler_t *p, od_t *od)
{
	p->env = ft_new();
	if (p->env == NULL)
		return -1;
	p->server = ft_io_new(p->env);
	if (p->server == NULL) {
		ft_free(p->env);
		return -1;
	}
	p->od = od;
	od_serverpool_init(&p->server_pool);
	od_clientpool_init(&p->client_pool);
	return 0;
}

int od_pooler_start(odpooler_t *p)
{
	int rc;
	rc = ft_create(p->env, od_pooler, p);
	if (rc < 0) {
		od_error(&p->od->log, "failed to create pooler fiber");
		return -1;
	}
	ft_start(p->env);
	return 0;
}
