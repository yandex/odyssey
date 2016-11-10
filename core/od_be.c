
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
#include "od_client.h"
#include "od_client_pool.h"
#include "od.h"
#include "od_pooler.h"
#include "od_be.h"

static int
od_beclose(odpooler_t *pooler, odserver_t *server)
{
	od_serverpool_set(&pooler->server_pool, server,
	                  OD_SUNDEF);
	if (server->io) {
		ft_close(server->io);
		server->io = NULL;
	}
	od_serverfree(server);
	return 0;
}

static int
od_beconnect(odpooler_t *pooler, odserver_t *server)
{
	(void)pooler;
	(void)server;
	return 0;
}

odserver_t*
od_bepop(odpooler_t *pooler, odscheme_route_t *route)
{
	/* try to fetch server from idle pool */
	odserver_t *server =
		od_serverpool_pop(&pooler->server_pool);
	if (server) {
		od_serverpool_set(&pooler->server_pool, server,
		                  OD_SACTIVE);
		return server;
	}
	/* create new server connection */
	server = od_serveralloc();
	if (server == NULL)
		return NULL;
	server->io = ft_io_new(pooler->env);
	if (server->io == NULL) {
		od_serverfree(server);
		return NULL;
	}
	server->route = route;
	int rc;
	rc = od_beconnect(pooler, server);
	if (rc == -1) {
		od_beclose(pooler, server);
		return NULL;
	}
	return server;
}
