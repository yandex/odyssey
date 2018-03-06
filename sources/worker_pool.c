
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
#include "sources/config.h"
#include "sources/config_mgr.h"
#include "sources/config_reader.h"
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
#include "sources/router_cancel.h"
#include "sources/router.h"
#include "sources/pooler.h"
#include "sources/worker.h"
#include "sources/worker_pool.h"
#include "sources/frontend.h"

void od_workerpool_init(od_workerpool_t *pool)
{
	pool->count       = 0;
	pool->round_robin = 0;
	pool->pool        = NULL;
}

int od_workerpool_start(od_workerpool_t *pool, od_system_t *system, int count)
{
	pool->pool = malloc(sizeof(od_worker_t) * count);
	if (pool->pool == NULL)
		return -1;
	pool->count = count;
	int i;
	for (i = 0; i < count; i++) {
		od_worker_t *worker = &pool->pool[i];
		od_worker_init(worker, system, i);
		int rc;
		rc = od_worker_start(worker);
		if (rc == -1)
			return -1;
	}
	return 0;
}

void od_workerpool_feed(od_workerpool_t *pool, machine_msg_t *msg)
{
	int next = pool->round_robin;
	if (pool->round_robin >= pool->count) {
		pool->round_robin = 0;
		next = 0;
	}
	pool->round_robin++;

	od_worker_t *worker;
	worker = &pool->pool[next];
	machine_channel_write(worker->task_channel, msg);
}
