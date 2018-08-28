
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <assert.h>

#include <machinarium.h>
#include <kiwi.h>
#include <odyssey.h>

void
od_server_pool_init(od_server_pool_t *pool)
{
	pool->count_idle = 0;
	pool->count_active = 0;
	pool->count_expire = 0;
	od_list_init(&pool->idle);
	od_list_init(&pool->active);
	od_list_init(&pool->expire);
	od_list_init(&pool->link);
}

void
od_server_pool_free(od_server_pool_t *pool)
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

void
od_server_pool_set(od_server_pool_t *pool, od_server_t *server,
                   od_server_state_t state)
{
	if (server->state == state)
		return;
	switch (server->state) {
	case OD_SERVER_UNDEF:
		break;
	case OD_SERVER_EXPIRE:
		pool->count_expire--;
		break;
	case OD_SERVER_IDLE:
		pool->count_idle--;
		break;
	case OD_SERVER_ACTIVE:
		pool->count_active--;
		break;
	}
	od_list_t *target = NULL;
	switch (state) {
	case OD_SERVER_UNDEF:
		break;
	case OD_SERVER_EXPIRE:
		target = &pool->expire;
		pool->count_expire++;
		break;
	case OD_SERVER_IDLE:
		target = &pool->idle;
		pool->count_idle++;
		break;
	case OD_SERVER_ACTIVE:
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
od_server_pool_next(od_server_pool_t *pool, od_server_state_t state)
{
	int target_count = 0;
	od_list_t *target = NULL;
	switch (state) {
	case OD_SERVER_IDLE:
		target_count = pool->count_idle;
		target = &pool->idle;
		break;
	case OD_SERVER_EXPIRE:
		target_count = pool->count_expire;
		target = &pool->expire;
		break;
	case OD_SERVER_ACTIVE:
		target_count = pool->count_active;
		target = &pool->active;
		break;
	case OD_SERVER_UNDEF:
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
od_server_pool_foreach(od_server_pool_t *pool, od_server_state_t state,
                       od_server_pool_cb_t callback,
                       void *arg)
{
	od_list_t *target = NULL;
	switch (state) {
	case OD_SERVER_IDLE:   target = &pool->idle;
		break;
	case OD_SERVER_EXPIRE: target = &pool->expire;
		break;
	case OD_SERVER_ACTIVE: target = &pool->active;
		break;
	case OD_SERVER_UNDEF:  assert(0);
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
