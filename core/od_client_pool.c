
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
#include "od_client.h"
#include "od_client_pool.h"

void od_clientpool_init(odclient_pool_t *p)
{
	od_listinit(&p->list);
	p->count = 0;
}

void od_clientpool_free(odclient_pool_t *p)
{
	od_client_t *client;
	od_list_t *i, *n;
	od_listforeach_safe(&p->list, i, n) {
		client = od_container_of(i, od_client_t, link);
		/* ... */
		od_clientfree(client);
	}
}

od_client_t*
od_clientpool_new(odclient_pool_t *p)
{
	od_client_t *c = od_clientalloc();
	if (c == NULL)
		return NULL;
	od_listappend(&p->list, &c->link);
	p->count++;
	return c;
}

void
od_clientpool_unlink(odclient_pool_t *p, od_client_t *c)
{
	assert(p->count > 0);
	od_listunlink(&c->link);
	p->count--;
	od_clientfree(c);
}
