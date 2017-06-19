
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
#include <signal.h>

#include <machinarium.h>
#include <shapito.h>

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
#include "od_frontend.h"

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
				od_error_client(&instance->log, client->id, "relay",
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

	od_log(&instance->log, "relay: stopped");
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
		od_error(&instance->log, "relay", "failed to create task queue");
		return -1;
	}
	char name[32];
	snprintf(name, sizeof(name), "relay: %d", relay->id);
	relay->machine = machine_create(name, od_relay, relay);
	if (relay->machine == -1) {
		machine_queue_free(relay->task_queue);
		od_error(&instance->log, "relay", "failed to start relay");
		return -1;
	}
	return 0;
}
