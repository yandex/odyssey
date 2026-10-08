/*
 * Odyssey.
 *
 * Dedicated workers for the initial TLS negotiation stage.
 */

#include <odyssey.h>
#include <stdatomic.h>
#include <machinarium/machine.h>
#include <frontend.h>
#include <global.h>
#include <instance.h>
#include <msg.h>
#include <tls_workers.h>

typedef struct {
	int64_t machine;
	machine_channel_t *channel;
	/* Reserved at startup, so shutdown does not need an allocation. */
	machine_msg_t *stop;
} od_tls_worker_t;

static od_tls_worker_t *tls_workers;
static size_t tls_workers_count;
static atomic_uint_fast64_t tls_workers_next;

static void od_tls_worker_main(void *arg)
{
	od_tls_worker_t *worker = arg;
	for (;;) {
		machine_msg_t *msg =
			machine_channel_read(worker->channel, UINT32_MAX);
		if (msg == NULL) {
			continue;
		}
		if (machine_msg_type(msg) == OD_MSG_SHUTDOWN) {
			machine_msg_free(msg);
			/* Active negotiation coroutines finish and transfer their
			 * clients before this machine exits naturally. */
			return;
		}
		od_client_t *client = *(od_client_t **)machine_msg_data(msg);
		int64_t id = machine_coroutine_create_named(od_frontend_tls,
							    msg, "tls startup");
		if (id == -1) {
			od_global_t *global = client->global;
			od_error(&global->instance->logger, "tls", client, NULL,
				 "failed to create negotiation coroutine");
			od_frontend_close(client);
			od_routing_slot_release(global);
			machine_msg_free(msg);
		} else {
			client->coroutine_id = id;
		}
	}
}

int od_tls_workers_init(size_t count)
{
	if (count == 0) {
		return 0;
	}
	tls_workers = od_calloc(count, sizeof(*tls_workers));
	if (tls_workers == NULL) {
		return -1;
	}
	atomic_store(&tls_workers_next, 0);
	for (size_t i = 0; i < count; i++) {
		od_tls_worker_t *worker = &tls_workers[i];
		worker->channel = machine_channel_create();
		worker->stop = machine_msg_create(0);
		if (worker->channel == NULL || worker->stop == NULL) {
			if (worker->channel) {
				machine_channel_free(worker->channel);
			}
			machine_msg_free_safe(worker->stop);
			goto error;
		}
		machine_msg_set_type(worker->stop, OD_MSG_SHUTDOWN);
		char name[32];
		od_snprintf(name, sizeof(name), "tls: %zu", i);
		worker->machine =
			machine_create(name, od_tls_worker_main, worker);
		if (worker->machine == -1) {
			machine_channel_free(worker->channel);
			machine_msg_free(worker->stop);
			goto error;
		}
		tls_workers_count++;
	}
	return 0;

error:
	od_tls_workers_destroy();
	return -1;
}

void od_tls_workers_destroy(void)
{
	/* Acceptors must have stopped, and regular workers must remain alive. */
	for (size_t i = 0; i < tls_workers_count; i++) {
		machine_channel_write(tls_workers[i].channel,
				      tls_workers[i].stop);
	}
	for (size_t i = 0; i < tls_workers_count; i++) {
		machine_wait_nb(tls_workers[i].machine);
		machine_channel_free(tls_workers[i].channel);
	}
	od_free(tls_workers);
	tls_workers = NULL;
	tls_workers_count = 0;
}

int od_tls_workers_enabled(void)
{
	return tls_workers_count > 0;
}

void od_tls_workers_feed(machine_msg_t *msg)
{
	uint64_t next = atomic_fetch_add(&tls_workers_next, 1);
	machine_channel_write(tls_workers[next % tls_workers_count].channel,
			      msg);
}
