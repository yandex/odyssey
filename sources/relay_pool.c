
/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <signal.h>

#include <machinarium.h>
#include <shapito.h>

#include "sources/macro.h"
#include "sources/version.h"
#include "sources/atomic.h"
#include "sources/util.h"
#include "sources/error.h"
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/logger.h"
#include "sources/daemon.h"
#include "sources/scheme.h"
#include "sources/scheme_mgr.h"
#include "sources/config.h"
#include "sources/msg.h"
#include "sources/system.h"
#include "sources/server.h"
#include "sources/server_pool.h"
#include "sources/client.h"
#include "sources/client_pool.h"
#include "sources/route_id.h"
#include "sources/route.h"
#include "sources/route_pool.h"
#include "sources/instance.h"
#include "sources/router.h"
#include "sources/pooler.h"
#include "sources/relay.h"
#include "sources/relay_pool.h"
#include "sources/frontend.h"

int od_relaypool_init(od_relaypool_t *pool, od_system_t *system, int count)
{
	pool->round_robin = 0;
	pool->count = count;
	pool->pool = malloc(sizeof(od_relay_t) * count);
	if (pool->pool == NULL)
		return -1;
	int i;
	for (i = 0; i < count; i++) {
		od_relay_t *relay = &pool->pool[i];
		od_relay_init(relay, system, i);
	}
	return 0;
}

int od_relaypool_start(od_relaypool_t *pool)
{
	int i;
	for (i = 0; i < pool->count; i++) {
		od_relay_t *relay = &pool->pool[i];
		int rc;
		rc = od_relay_start(relay);
		if (rc == -1)
			return -1;
	}
	return 0;
}

void od_relaypool_feed(od_relaypool_t *pool, machine_msg_t *msg)
{
	int next = pool->round_robin;
	if (pool->round_robin >= pool->count) {
		pool->round_robin = 0;
		next = 0;
	}
	pool->round_robin++;

	od_relay_t *relay;
	relay = &pool->pool[next];
	machine_channel_write(relay->task_channel, msg);
}
