
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

static odserver_t*
od_beconnect(odpooler_t *pooler, sobestartup_t *startup)
{
	(void)pooler;
	(void)startup;
	return NULL;
}

odserver_t*
od_bepop(odpooler_t *pooler, sobestartup_t *startup)
{
	odserver_t *server = od_serverpool_pop(&pooler->server_pool);
	if (server) {
		od_serverpool_set(&pooler->server_pool, server, OD_SACTIVE);
		return server;
	}
	server = od_beconnect(pooler, startup);
	return server;
}
