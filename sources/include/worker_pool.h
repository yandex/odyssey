#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium/machinarium.h>

#include <types.h>
#include <stdatomic.h>
#include <worker.h>
#include <od_memory.h>

struct od_worker_pool {
	od_worker_t *pool;
	atomic_uint_fast64_t round_robin;
	uint32_t count;
};

static inline void od_worker_pool_init(od_worker_pool_t *pool)
{
	pool->count = 0;
	pool->round_robin = 0;
	pool->pool = NULL;
}

static inline od_retcode_t od_worker_pool_start(od_worker_pool_t *pool,
						od_global_t *global,
						uint32_t count)
{
	pool->pool = od_malloc(sizeof(od_worker_t) * count);
	if (pool->pool == NULL) {
		return -1;
	}
	pool->count = count;
	uint32_t i;
	for (i = 0; i < count; i++) {
		od_worker_t *worker = &pool->pool[i];
		od_worker_init(worker, global, i);
		int rc;
		rc = od_worker_start(worker);
		if (rc == -1) {
			return NOT_OK_RESPONSE;
		}
	}
	return 0;
}

static inline void od_worker_pool_shutdown(od_worker_pool_t *pool)
{
	for (uint32_t i = 0; i < pool->count; ++i) {
		od_worker_t *worker = &pool->pool[i];
		od_worker_shutdown(worker);
	}
}

static inline void
od_worker_pool_wait_gracefully_shutdown(od_worker_pool_t *pool)
{
	for (uint32_t i = 0; i < pool->count; i++) {
		od_worker_t *worker = &pool->pool[i];
		int rc = machine_wait(worker->machine);
		if (rc != MM_OK_RETCODE) {
			return;
		}

		machine_channel_free(worker->task_channel);
	}

	od_free(pool->pool);
}

static inline void od_worker_pool_feed(od_worker_pool_t *pool,
				       machine_msg_t *msg)
{
	uint64_t next;
	uint64_t oldValue;

	for (;;) {
		oldValue = atomic_load(&pool->round_robin);
		next = oldValue + 1 == pool->count ? 0 : oldValue + 1;

		if (atomic_compare_exchange_strong(&pool->round_robin,
						   &oldValue, next)) {
			break;
		}
	}

	od_worker_t *worker;
	worker = &pool->pool[next];
	machine_channel_write(worker->task_channel, msg);
}
