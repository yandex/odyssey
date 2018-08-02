
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
#include "sources/config_reader.h"
#include "sources/msg.h"
#include "sources/global.h"
#include "sources/stat.h"
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
#include "sources/system.h"
#include "sources/worker.h"
#include "sources/frontend.h"

static inline void
od_worker(void *arg)
{
	od_worker_t *worker = arg;
	od_instance_t *instance = worker->global->instance;

	for (;;)
	{
		machine_msg_t *msg;
		msg = machine_channel_read(worker->task_channel, UINT32_MAX);
		if (msg == NULL)
			break;

		od_msg_t msg_type;
		msg_type = machine_msg_get_type(msg);
		switch (msg_type) {
		case OD_MCLIENT_NEW:
		{
			od_client_t *client;
			client = *(od_client_t**)machine_msg_get_data(msg);
			client->global = worker->global;
			int64_t coroutine_id;
			coroutine_id = machine_coroutine_create(od_frontend, client);
			if (coroutine_id == -1) {
				od_error(&instance->logger, "worker", client, NULL,
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

	od_log(&instance->logger, "worker", NULL, NULL, "stopped");
}

void od_worker_init(od_worker_t *worker, od_global_t *global, int id)
{
	worker->machine = -1;
	worker->id = id;
	worker->global = global;
}

int od_worker_start(od_worker_t *worker)
{
	od_instance_t *instance = worker->global->instance;

	worker->task_channel = machine_channel_create(instance->is_shared);
	if (worker->task_channel == NULL) {
		od_error(&instance->logger, "worker", NULL, NULL,
		         "failed to create task channel");
		return -1;
	}
	if (instance->is_shared) {
		char name[32];
		od_snprintf(name, sizeof(name), "worker: %d", worker->id);
		worker->machine = machine_create(name, od_worker, worker);
		if (worker->machine == -1) {
			machine_channel_free(worker->task_channel);
			od_error(&instance->logger, "worker", NULL, NULL,
			         "failed to start worker");
			return -1;
		}
	} else {
		int64_t coroutine_id;
		coroutine_id = machine_coroutine_create(od_worker, worker);
		if (coroutine_id == -1) {
			od_error(&instance->logger, "worker", NULL, NULL,
			         "failed to create worker coroutine");
			machine_channel_free(worker->task_channel);
			return -1;
		}
	}
	return 0;
}
