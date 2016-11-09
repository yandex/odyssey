
/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <flint.h>

#include "od_macro.h"
#include "od_list.h"
#include "od_log.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"
#include "od_server.h"
#include "od_pool.h"

void od_poolinit(odpool_t *p)
{
	p->count_active = 0;
	p->count_connect = 0;
	p->count_idle = 0;
	od_listinit(&p->active);
	od_listinit(&p->connect);
	od_listinit(&p->idle);
	od_listinit(&p->link);
}

void od_poolfree(odpool_t *p)
{
	odserver_t *server;
	odlist_t *i, *n;
	od_listforeach_safe(&p->idle, i, n) {
		server = od_container_of(i, odserver_t, link);
		od_serverfree(server);
	}
	od_listforeach_safe(&p->connect, i, n) {
		server = od_container_of(i, odserver_t, link);
		od_serverfree(server);
	}
	od_listforeach_safe(&p->active, i, n) {
		server = od_container_of(i, odserver_t, link);
		od_serverfree(server);
	}
}

odserver_t*
od_poolpop(odpool_t *p)
{
	if (p->count_idle == 0)
		return NULL;
	odserver_t *server;
	server = od_container_of(p->idle.next, odserver_t, link);
	assert(server->state == OD_SIDLE);
	return server;
}

void od_poolset(odpool_t *p, odserver_t *server, odserver_state_t state)
{
	switch (server->state) {
	case OD_SUNDEF:
		break;
	case OD_SIDLE:
		p->count_idle--;
	case OD_SCONNECT:
		p->count_connect--;
		break;
	case OD_SACTIVE:
		p->count_active--;
		break;
	}
	odlist_t *target = NULL;
	switch (state) {
	case OD_SUNDEF:
		break;
	case OD_SIDLE:
		target = &p->idle;
		p->count_idle++;
		break;
	case OD_SCONNECT:
		target = &p->connect;
		p->count_connect++;
		break;
	case OD_SACTIVE:
		target = &p->active;
		p->count_active++;
		break;
	}
	od_listunlink(&server->link);
	od_listinit(&server->link);
	if (target)
		od_listappend(target, &server->link);
	server->state = state;
}
