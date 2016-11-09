
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
#include "od_client.h"
#include "od_pool.h"
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
	while (1) {
		ftio_t client_io;
		rc = ft_accept(p->server, &client_io);
		if (rc < 0) {
			od_error(&env->log, "accept failed");
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
	od_poolinit(&p->pool);
	return 0;
}

int od_pooler_start(odpooler_t *p)
{
	ft_create(p->env, od_pooler, p);
	ft_start(p->env);
	return 0;
}
