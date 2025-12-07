
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>

#include <worker.h>
#include <global.h>
#include <thread_global.h>
#include <client.h>
#include <instance.h>
#include <msg.h>
#include <frontend.h>
#include <router.h>

#ifdef PROM_FOUND
#include <cron.h>
#include <prom_metric.h>
#endif

static inline void od_worker(void *arg)
{
	od_worker_t *worker = arg;
	od_instance_t *instance = worker->global->instance;
	od_router_t *router = worker->global->router;

	/* thread global initialization */
	od_thread_global **gl = od_thread_global_get();
	od_retcode_t rc = od_thread_global_init(gl);

	if (rc != OK_RESPONSE) {
		/* TODO: set errno */
		od_fatal(&instance->logger, "worker_init", NULL, NULL,
			 "failed to init worker thread info");
		return;
	}

	(*gl)->wid = worker->id;

	bool run = true;

	while (run) {
		uint32_t task_wait_timout_ms = 10 * 1000;

		machine_msg_t *msg;
		/* Inverse priorities of cliend routing to decrease chances of timeout */
		msg = machine_channel_read_back(worker->task_channel,
						task_wait_timout_ms);
		if (msg == NULL) {
			od_log(&instance->logger, "worker", NULL, NULL,
			       "worker[%d]: task channel is empty for %u ms",
			       worker->id, task_wait_timout_ms);
			continue;
		}

		od_msg_t msg_type;
		msg_type = machine_msg_type(msg);
		switch (msg_type) {
		case OD_MSG_CLIENT_NEW: {
			od_client_t *client;
			client = *(od_client_t **)machine_msg_data(msg);
			client->global = worker->global;

			/* for NULL-terminator and prefix, just in case */
			char coro_name[10 + OD_ID_LEN];
			od_id_write_to_string(&client->id, coro_name,
					      10 + OD_ID_LEN);

			int64_t coroutine_id;
			coroutine_id = machine_coroutine_create_named(
				od_frontend, client, coro_name);
			if (coroutine_id == -1) {
				od_error(&instance->logger, "worker", client,
					 NULL, "failed to create coroutine");
				od_io_close(&client->io);
				od_client_free(client);
				od_atomic_u32_dec(&router->clients_routing);
				break;
			}
			client->coroutine_id = coroutine_id;

			worker->clients_processed++;
			break;
		}
		case OD_MSG_STAT: {
			uint64_t count_coroutine = 0;
			uint64_t count_coroutine_cache = 0;
			uint64_t msg_allocated = 0;
			uint64_t msg_cache_count = 0;
			uint64_t msg_cache_gc_count = 0;
			uint64_t msg_cache_size = 0;
			machine_stat(&count_coroutine, &count_coroutine_cache,
				     &msg_allocated, &msg_cache_count,
				     &msg_cache_gc_count, &msg_cache_size);
#ifdef PROM_FOUND
			od_prom_metrics_write_worker_stat(
				((od_cron_t *)(worker->global->cron))->metrics,
				worker->id, msg_allocated, msg_cache_count,
				msg_cache_gc_count, msg_cache_size,
				count_coroutine, count_coroutine_cache,
				worker->clients_processed);
#endif
			od_log(&instance->logger, "stats", NULL, NULL,
			       "worker[%d]: msg (%" PRIu64
			       " allocated, %" PRIu64 " cached, %" PRIu64
			       " freed, %" PRIu64 " cache_size), "
			       "coroutines (%" PRIu64 " active, %" PRIu64
			       " cached), clients_processed: %" PRIu64,
			       worker->id, msg_allocated, msg_cache_count,
			       msg_cache_gc_count, msg_cache_size,
			       count_coroutine, count_coroutine_cache,
			       worker->clients_processed);
			break;
		}
		case OD_MSG_SHUTDOWN:
			od_log(&instance->logger, "worker", NULL, NULL,
			       "worker[%d]: shutdown message received",
			       worker->id);
			run = false;
			break;
		default:
			assert(0);
			break;
		}

		machine_msg_free(msg);
	}

	od_thread_global_free(*gl);

	od_log(&instance->logger, "worker", NULL, NULL, "worker[%d] stopped",
	       worker->id);
}

void od_worker_init(od_worker_t *worker, od_global_t *global, int id)
{
	worker->machine = -1;
	worker->id = id;
	worker->global = global;
	worker->clients_processed = 0;
}

int od_worker_start(od_worker_t *worker)
{
	od_instance_t *instance = worker->global->instance;

	worker->task_channel = machine_channel_create();
	if (worker->task_channel == NULL) {
		od_error(&instance->logger, "worker", NULL, NULL,
			 "failed to create task channel");
		return -1;
	}

	char name[32];
	od_snprintf(name, sizeof(name), "worker: %d", worker->id);
	worker->machine = machine_create(name, od_worker, worker);
	if (worker->machine == -1) {
		machine_channel_free(worker->task_channel);
		od_error(&instance->logger, "worker", NULL, NULL,
			 "failed to start worker");
		return -1;
	}

	return 0;
}

void od_worker_shutdown(od_worker_t *worker)
{
	machine_msg_t *msg;
	msg = machine_msg_create(0);
	machine_msg_set_type(msg, OD_MSG_SHUTDOWN);
	machine_channel_write(worker->task_channel, msg);
}
