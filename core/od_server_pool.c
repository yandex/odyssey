
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
#include <assert.h>

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

void od_serverpool_init(od_serverpool_t *p)
{
	p->count_active = 0;
	p->count_connect = 0;
	p->count_reset = 0;
	p->count_expire = 0;
	p->count_close = 0;
	p->count_idle = 0;
	od_listinit(&p->active);
	od_listinit(&p->connect);
	od_listinit(&p->reset);
	od_listinit(&p->idle);
	od_listinit(&p->expire);
	od_listinit(&p->close);
	od_listinit(&p->link);
}

void od_serverpool_free(od_serverpool_t *p)
{
	od_server_t *server;
	od_list_t *i, *n;
	od_listforeach_safe(&p->idle, i, n) {
		server = od_container_of(i, od_server_t, link);
		od_serverfree(server);
	}
	od_listforeach_safe(&p->expire, i, n) {
		server = od_container_of(i, od_server_t, link);
		od_serverfree(server);
	}
	od_listforeach_safe(&p->close, i, n) {
		server = od_container_of(i, od_server_t, link);
		od_serverfree(server);
	}
	od_listforeach_safe(&p->connect, i, n) {
		server = od_container_of(i, od_server_t, link);
		od_serverfree(server);
	}
	od_listforeach_safe(&p->reset, i, n) {
		server = od_container_of(i, od_server_t, link);
		od_serverfree(server);
	}
	od_listforeach_safe(&p->active, i, n) {
		server = od_container_of(i, od_server_t, link);
		od_serverfree(server);
	}
}

void od_serverpool_set(od_serverpool_t *p, od_server_t *server,
                       od_serverstate_t state)
{
	if (server->state == state)
		return;
	switch (server->state) {
	case OD_SUNDEF:
		break;
	case OD_SEXPIRE:
		p->count_expire--;
		break;
	case OD_SCLOSE:
		p->count_close--;
		break;
	case OD_SIDLE:
		p->count_idle--;
		break;
	case OD_SCONNECT:
		p->count_connect--;
		break;
	case OD_SRESET:
		p->count_reset--;
		break;
	case OD_SACTIVE:
		p->count_active--;
		break;
	}
	od_list_t *target = NULL;
	switch (state) {
	case OD_SUNDEF:
		break;
	case OD_SEXPIRE:
		target = &p->expire;
		p->count_expire++;
		break;
	case OD_SCLOSE:
		target = &p->close;
		p->count_close++;
		break;
	case OD_SIDLE:
		target = &p->idle;
		p->count_idle++;
		break;
	case OD_SCONNECT:
		target = &p->connect;
		p->count_connect++;
		break;
	case OD_SRESET:
		target = &p->reset;
		p->count_reset++;
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

od_server_t*
od_serverpool_next(od_serverpool_t *p, od_serverstate_t state)
{
	od_list_t *target = NULL;
	switch (state) {
	case OD_SIDLE:    target = &p->idle;
		break;
	case OD_SEXPIRE:  target = &p->expire;
		break;
	case OD_SCLOSE:   target = &p->close;
		break;
	case OD_SCONNECT: target = &p->connect;
		break;
	case OD_SRESET:   target = &p->reset;
		break;
	case OD_SACTIVE:  target = &p->active;
		break;
	case OD_SUNDEF:   assert(0);
		break;
	}
	if (od_listempty(target))
		return NULL;
	od_server_t *server;
	server = od_container_of(target->next, od_server_t, link);
	return server;
}

od_server_t*
od_serverpool_foreach(od_serverpool_t *p, od_serverstate_t state,
                      od_serverpool_cb_t callback,
                      void *arg)
{
	od_list_t *target = NULL;
	switch (state) {
	case OD_SIDLE:    target = &p->idle;
		break;
	case OD_SEXPIRE:  target = &p->expire;
		break;
	case OD_SCLOSE:   target = &p->close;
		break;
	case OD_SCONNECT: target = &p->connect;
		break;
	case OD_SRESET:   target = &p->reset;
		break;
	case OD_SACTIVE:  target = &p->active;
		break;
	case OD_SUNDEF:   assert(0);
		break;
	}
	od_server_t *server;
	od_list_t *i, *n;
	od_listforeach_safe(target, i, n) {
		server = od_container_of(i, od_server_t, link);
		int rc;
		rc = callback(server, arg);
		if (rc) {
			return server;
		}
	}
	return NULL;
}
