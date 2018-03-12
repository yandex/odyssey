
/*
 * Odyssey.
 *
 * Advanced PostgreSQL connection pooler.
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

#include "sources/macro.h"
#include "sources/atomic.h"
#include "sources/util.h"
#include "sources/error.h"
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/logger.h"
#include "sources/config.h"
#include "sources/config_mgr.h"
#include "sources/config_reader.h"
#include "sources/system.h"
#include "sources/server.h"
#include "sources/server_pool.h"

void od_serverpool_init(od_serverpool_t *pool)
{
	pool->count_idle = 0;
	pool->count_active = 0;
	pool->count_expire = 0;
	od_list_init(&pool->idle);
	od_list_init(&pool->active);
	od_list_init(&pool->expire);
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
	int target_count = 0;
	od_list_t *target = NULL;
	switch (state) {
	case OD_SIDLE:
		target_count = pool->count_idle;
		target = &pool->idle;
		break;
	case OD_SEXPIRE:
		target_count = pool->count_expire;
		target = &pool->expire;
		break;
	case OD_SACTIVE:
		target_count = pool->count_active;
		target = &pool->active;
		break;
	case OD_SUNDEF:
		assert(0);
		break;
	}
	if (target_count == 0)
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
	case OD_SIDLE:   target = &pool->idle;
		break;
	case OD_SEXPIRE: target = &pool->expire;
		break;
	case OD_SACTIVE: target = &pool->active;
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
