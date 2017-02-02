
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
#include "od_client.h"
#include "od_client_pool.h"

void od_clientpool_init(od_clientpool_t *p)
{
	p->count_active = 0;
	p->count_queue = 0;
	od_listinit(&p->active);
	od_listinit(&p->queue);
}

void od_clientpool_set(od_clientpool_t *p, od_client_t *client,
                       od_clientstate_t state)
{
	if (client->state == state)
		return;
	switch (client->state) {
	case OD_CUNDEF:
		break;
	case OD_CACTIVE:
		p->count_active--;
		break;
	case OD_CQUEUE:
		p->count_queue--;
		break;
	}
	od_list_t *target = NULL;
	switch (state) {
	case OD_CUNDEF:
		break;
	case OD_CACTIVE:
		target = &p->active;
		p->count_active++;
		break;
	case OD_CQUEUE:
		target = &p->queue;
		p->count_queue++;
		break;
	}
	od_listunlink(&client->link_pool);
	od_listinit(&client->link_pool);
	if (target)
		od_listpush(target, &client->link_pool);
	client->state = state;
}

od_client_t*
od_clientpool_next(od_clientpool_t *p, od_clientstate_t state)
{
	od_list_t *target = NULL;
	switch (state) {
	case OD_CACTIVE:
		target = &p->active;
		break;
	case OD_CQUEUE:
		target = &p->queue;
		break;
	case OD_CUNDEF:
		assert(0);
		break;
	}
	if (od_listempty(target))
		return NULL;
	od_client_t *client;
	client = od_container_of(target->next, od_client_t, link_pool);
	return client;
}
