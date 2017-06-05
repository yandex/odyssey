
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
#include <inttypes.h>
#include <signal.h>

#include <machinarium.h>
#include <soprano.h>

#include "od_macro.h"
#include "od_version.h"
#include "od_list.h"
#include "od_pid.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_daemon.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"
#include "od_msg.h"
#include "od_system.h"
#include "od_instance.h"

#include "od_server.h"
#include "od_server_pool.h"
#include "od_client.h"
#include "od_client_pool.h"
#include "od_route_id.h"
#include "od_route.h"
#include "od_route_pool.h"

#include "od_router.h"
#include "od_pooler.h"
#include "od_relay.h"
#include "od_relay_pool.h"
#include "od_frontend.h"

int od_relaypool_init(od_relaypool_t *pool, od_system_t *system, int count)
{
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
