
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
#include "sources/frontend.h"

static inline void
od_relay(void *arg)
{
	od_relay_t *relay = arg;
	od_instance_t *instance = relay->system->instance;

	for (;;)
	{
		machine_msg_t *msg;
		msg = machine_queue_get(relay->task_queue, UINT32_MAX);
		if (msg == NULL)
			break;

		od_msg_t msg_type;
		msg_type = machine_msg_get_type(msg);
		switch (msg_type) {
		case OD_MCLIENT_NEW:
		{
			od_client_t *client;
			client = *(od_client_t**)machine_msg_get_data(msg);
			client->system = relay->system;
			int64_t coroutine_id;
			coroutine_id = machine_coroutine_create(od_frontend, client);
			if (coroutine_id == -1) {
				od_error(&instance->logger, "relay", client, NULL,
				         "failed to create coroutine");
				machine_close(client->io);
				od_client_free(client);
				break;
			}
			client->coroutine_id = coroutine_id;
			break;
		}
		default:
			assert(0);
			break;
		}

		machine_msg_free(msg);
	}

	od_log(&instance->logger, "relay", NULL, NULL, "stopped");
}

void od_relay_init(od_relay_t *relay, od_system_t *system, int id)
{
	relay->machine = -1;
	relay->id = id;
	relay->system = system;
}

int od_relay_start(od_relay_t *relay)
{
	od_instance_t *instance = relay->system->instance;

	relay->task_queue = machine_queue_create();
	if (relay->task_queue == NULL) {
		od_error(&instance->logger, "relay", NULL, NULL,
		         "failed to create task queue");
		return -1;
	}
	char name[32];
	od_snprintf(name, sizeof(name), "relay: %d", relay->id);
	relay->machine = machine_create(name, od_relay, relay);
	if (relay->machine == -1) {
		machine_queue_free(relay->task_queue);
		od_error(&instance->logger, "relay", NULL, NULL,
		         "failed to start relay");
		return -1;
	}
	return 0;
}
