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
 */

#include <machinarium/spinlock.h>

#include <stdint.h>

typedef struct od_rate_limiter {
	/* Per second */
	uint64_t limit;

	int64_t tokens;

	/* last token update time */
	int64_t last;

	mm_spinlock_t lock;
} od_rate_limiter_t;

void od_rate_limiter_init(od_rate_limiter_t *lim, uint64_t limit);
void od_rate_limiter_destroy(od_rate_limiter_t *lim);

/*
 * WaitN blocks until the limiter permits n events to happen.
 * Returns 0 on success, -1 on timeout and
 * if n > 1 and the limiter cannot ever satisfy the request
 * with the given rate.
 *
 * If limit is 0, always returns -1 immediately.
 */
int od_rate_limiter_waitn(od_rate_limiter_t *lim, uint64_t n,
			  uint32_t timeout_ms);
