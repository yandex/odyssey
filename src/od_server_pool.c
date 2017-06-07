
/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include <machinarium.h>
#include <shapito.h>

#include "od_macro.h"
#include "od_list.h"
#include "od_pid.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"
#include "od_system.h"
#include "od_server.h"
#include "od_server_pool.h"

void od_serverpool_init(od_serverpool_t *pool)
{
	pool->count_idle = 0;
	pool->count_active = 0;
	pool->count_connect = 0;
	pool->count_expire = 0;
	od_list_init(&pool->idle);
	od_list_init(&pool->active);
	od_list_init(&pool->expire);
	od_list_init(&pool->connect);
	od_list_init(&pool->link);
}

void od_serverpool_free(od_serverpool_t *pool)
{
	od_server_t *server;
	od_list_t *i, *n;
	od_list_foreach_safe(&pool->idle, i, n) {
		server = od_container_of(i, od_server_t, link);
		od_server_free(server);
	}
	od_list_foreach_safe(&pool->connect, i, n) {
		server = od_container_of(i, od_server_t, link);
		od_server_free(server);
	}
	od_list_foreach_safe(&pool->expire, i, n) {
		server = od_container_of(i, od_server_t, link);
		od_server_free(server);
	}
	od_list_foreach_safe(&pool->active, i, n) {
		server = od_container_of(i, od_server_t, link);
		od_server_free(server);
	}
}

void od_serverpool_set(od_serverpool_t *pool, od_server_t *server,
                       od_serverstate_t state)
{
	if (server->state == state)
		return;
	switch (server->state) {
	case OD_SUNDEF:
		break;
	case OD_SEXPIRE:
		pool->count_expire--;
		break;
	case OD_SIDLE:
		pool->count_idle--;
		break;
	case OD_SCONNECT:
		pool->count_connect--;
		break;
	case OD_SACTIVE:
		pool->count_active--;
		break;
	}
	od_list_t *target = NULL;
	switch (state) {
	case OD_SUNDEF:
		break;
	case OD_SEXPIRE:
		target = &pool->expire;
		pool->count_expire++;
		break;
	case OD_SCONNECT:
		target = &pool->connect;
		pool->count_connect++;
		break;
	case OD_SIDLE:
		target = &pool->idle;
		pool->count_idle++;
		break;
	case OD_SACTIVE:
		target = &pool->active;
		pool->count_active++;
		break;
	}
	od_list_unlink(&server->link);
	od_list_init(&server->link);
	if (target)
		od_list_push(target, &server->link);
	server->state = state;
}

od_server_t*
od_serverpool_next(od_serverpool_t *pool, od_serverstate_t state)
{
	od_list_t *target = NULL;
	switch (state) {
	case OD_SIDLE:    target = &pool->idle;
		break;
	case OD_SEXPIRE:  target = &pool->expire;
		break;
	case OD_SACTIVE:  target = &pool->active;
		break;
	case OD_SCONNECT: target = &pool->connect;
		break;
	case OD_SUNDEF:  assert(0);
		break;
	}
	if (od_list_empty(target))
		return NULL;
	od_server_t *server;
	server = od_container_of(target->next, od_server_t, link);
	return server;
}

od_server_t*
od_serverpool_foreach(od_serverpool_t *pool,
                      od_serverstate_t state,
                      od_serverpool_cb_t callback,
                      void *arg)
{
	od_list_t *target = NULL;
	switch (state) {
	case OD_SIDLE:    target = &pool->idle;
		break;
	case OD_SEXPIRE:  target = &pool->expire;
		break;
	case OD_SACTIVE:  target = &pool->active;
		break;
	case OD_SCONNECT: target = &pool->connect;
		break;
	case OD_SUNDEF:  assert(0);
		break;
	}
	od_server_t *server;
	od_list_t *i, *n;
	od_list_foreach_safe(target, i, n) {
		server = od_container_of(i, od_server_t, link);
		int rc;
		rc = callback(server, arg);
		if (rc) {
			return server;
		}
	}
	return NULL;
}
