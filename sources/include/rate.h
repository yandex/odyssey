#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

/*
 * Minimal token-bucket rate limiter, inspired by Go's time/rate package.
 * No burst support (burst is effectively 1). The limiter refills tokens
 * at a fixed rate of `limit` events per second.
 *
 * Waiting is performed via mm_wait_list_wait (cooperative), so the limiter
 * must be used from machinarium coroutines, not raw threads.
 */

#include <machinarium/wait_list.h>
#include <machinarium/sleep_lock.h>

#include <stdint.h>

typedef struct od_rate_limiter {
	/* Per second */
	uint64_t limit;

	int64_t tokens;

	/* last token update time */
	int64_t last;

	mm_wait_list_t *waiters;

	mm_sleeplock_t lock;
} od_rate_limiter_t;

od_rate_limiter_t *od_rate_limiter_create(uint64_t limit);

/* Destroy and free a limiter. */
void od_rate_limiter_destroy(od_rate_limiter_t *lim);
void od_rate_limiter_free(od_rate_limiter_t *lim);

/*
 * WaitN blocks until the limiter permits n events to happen.
 * Returns 0 on success, -1 on timeout and
 * if n > 1 and the limiter cannot ever satisfy the request
 * with the given rate.
 *
 * If limit is 0, always returns -1 immediately.
 */
int od_rate_limiter_waitn(od_rate_limiter_t *lim, uint64_t n);
