#ifndef ODYSSEY_WORKER_POOL_H
#define ODYSSEY_WORKER_POOL_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include "machinarium.h"

typedef struct od_worker_pool od_worker_pool_t;

struct od_worker_pool
{
	od_worker_t *pool;
	int round_robin;
	int count;
};

static inline void
od_worker_pool_init(od_worker_pool_t *pool)
{
	pool->count       = 0;
	pool->round_robin = 0;
	pool->pool        = NULL;
}

static inline int
od_worker_pool_start(od_worker_pool_t *pool, od_global_t *global, int count)
{
	pool->pool = malloc(sizeof(od_worker_t) * count);
	if (pool->pool == NULL)
		return -1;
	pool->count = count;
	int i;
	for (i = 0; i < count; i++) {
		od_worker_t *worker = &pool->pool[i];
		od_worker_init(worker, global, i);
		int rc;
		rc = od_worker_start(worker);
		if (rc == -1)
			return -1;
	}
	return 0;
}

static inline void
od_worker_pool_stop(od_worker_pool_t *pool)
{
	for (int i = 0; i < pool->count; i++) {
		od_worker_t *worker = &pool->pool[i];
		machine_stop(worker->machine);
	}
}

static inline void
od_worker_pool_wait()
{
	machine_sleep(1);
}

static inline void
od_worker_pool_wait_gracefully_shutdown(od_worker_pool_t *pool)
{
	//	// In fact we cannot wait anything here - machines may be in epoll
	// waiting
	//	// No new TLS handshakes should be initiated, so, just wait a bit.
	//	machine_sleep(1);
	for (int i = 0; i < pool->count; i++) {
		od_worker_t *worker = &pool->pool[i];
		int rc              = machine_wait(worker->machine);
		if (rc != MM_OK_RETCODE)
			return;
	}
}

static inline void
od_worker_pool_feed(od_worker_pool_t *pool, machine_msg_t *msg)
{
	int next = pool->round_robin;
	if (pool->round_robin >= pool->count) {
		pool->round_robin = 0;
		next              = 0;
	}
	pool->round_robin++;

	od_worker_t *worker;
	worker = &pool->pool[next];
	machine_channel_write(worker->task_channel, msg);
}

#endif /* ODYSSEY_WORKER_POOL_H */
