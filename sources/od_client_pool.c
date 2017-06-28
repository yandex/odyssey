
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
#include "od_id.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"
#include "od_system.h"
#include "od_server.h"
#include "od_server_pool.h"
#include "od_client.h"
#include "od_client_pool.h"

void od_clientpool_init(od_clientpool_t *pool)
{
	pool->count_active  = 0;
	pool->count_queue   = 0;
	pool->count_pending = 0;
	od_list_init(&pool->active);
	od_list_init(&pool->queue);
	od_list_init(&pool->pending);
}

void od_clientpool_set(od_clientpool_t *pool, od_client_t *client,
                       od_clientstate_t state)
{
	if (client->state == state)
		return;
	switch (client->state) {
	case OD_CUNDEF:
		break;
	case OD_CACTIVE:
		pool->count_active--;
		break;
	case OD_CQUEUE:
		pool->count_queue--;
		break;
	case OD_CPENDING:
		pool->count_pending--;
		break;
	}
	od_list_t *target = NULL;
	switch (state) {
	case OD_CUNDEF:
		break;
	case OD_CACTIVE:
		target = &pool->active;
		pool->count_active++;
		break;
	case OD_CQUEUE:
		target = &pool->queue;
		pool->count_queue++;
		break;
	case OD_CPENDING:
		target = &pool->pending;
		pool->count_pending++;
		break;
	}
	od_list_unlink(&client->link_pool);
	od_list_init(&client->link_pool);
	if (target)
		od_list_append(target, &client->link_pool);
	client->state = state;
}

od_client_t*
od_clientpool_next(od_clientpool_t *pool, od_clientstate_t state)
{
	od_list_t *target = NULL;
	switch (state) {
	case OD_CACTIVE:
		target = &pool->active;
		break;
	case OD_CQUEUE:
		target = &pool->queue;
		break;
	case OD_CPENDING:
		target = &pool->pending;
		break;
	case OD_CUNDEF:
		assert(0);
		break;
	}
	if (od_list_empty(target))
		return NULL;
	od_client_t *client;
	client = od_container_of(target->next, od_client_t, link_pool);
	return client;
}
