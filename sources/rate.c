/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

/*
* Implementation borrowed from golang x/rate.
*/

#include <odyssey.h>

#include <od_memory.h>
#include <arpa/inet.h>
#include <pthread.h>

#include <machinarium/machine.h>
#include <kiwi/header.h>

#include <rate.h>

/*
 * Simple token-bucket rate limiter
 *
 * 1 token == 1e6.
 */

#define RATE_TOKEN_SCALE 1000000ULL

/*
 * How many time we need to accumulate that many token.
 * Keep this as separate func to support bursts (later).
 */
static inline int64_t od_rate_duration_from_tokens(uint64_t limit,
						   int64_t tokens)
{
	od_assert(limit);
	return tokens / (int64_t)limit;
}

/*
 * Advance the limiter clock to `now` and refill tokens.
 */
static inline int64_t od_rate_advance(od_rate_limiter_t *lim, int64_t now)
{
	int64_t last = lim->last;
	int64_t elapsed = now - last;
	/* CLOCK_MONOTONIC should guarantee < 0 is not possible.
    * anyway, check for zero this paranoic way. */
	if (elapsed <= 0) {
		return lim->tokens;
	}

	/* new tokens = elapsed_usec * limit per sec */
	int64_t delta = (elapsed * (int64_t)lim->limit);

	int64_t tokens = lim->tokens + delta;
	if (tokens > (int64_t)RATE_TOKEN_SCALE) {
		tokens = (int64_t)RATE_TOKEN_SCALE;
	}

	lim->tokens = tokens;
	lim->last = now;
	return tokens;
}

/* Limiter capacity is single token. limit is token 
* regenerate speed. For example, with lim = 100, two consecutive
* waits will be trottled with 10ms. */
void od_rate_limiter_init(od_rate_limiter_t *lim, uint64_t limit)
{
	lim->limit = limit;
	/* Fill just-created limiter with single token */
	lim->tokens = RATE_TOKEN_SCALE;
	lim->last = machine_time_us();
	mm_spinlock_init(&lim->lock);
}

void od_rate_limiter_destroy(od_rate_limiter_t *lim)
{
	od_assert(lim);
	mm_spinlock_destroy(&lim->lock);
}

/* NB: all current users call this with n == 1. */
int od_rate_limiter_waitn(od_rate_limiter_t *lim, uint64_t n,
			  uint32_t timeout_ms)
{
	od_assert(lim);

	if (lim->limit == 0) {
		return -1;
	}

	int64_t end_us;
	if (timeout_ms == UINT32_MAX) {
		end_us = UINT64_MAX;
	} else {
		end_us = machine_time_us() + timeout_ms * 1000;
	}

	int64_t now_us;
	int64_t wait_usec = 0;

	for (;;) {
		/* XXX: check for cancellation here ? */

		/* refresh and go */
		now_us = machine_time_us();
		if (now_us > end_us) {
			return -1;
		}

		mm_spinlock_lock(&lim->lock);

		int64_t tokens = od_rate_advance(lim, now_us);

		/* consume n tokens */
		int64_t remaining = tokens - (int64_t)(n * RATE_TOKEN_SCALE);

		if (remaining >= 0) {
			lim->tokens = remaining;
			mm_spinlock_unlock(&lim->lock);
			return 0;
		}
		mm_spinlock_unlock(&lim->lock);

		/* not enough tokens: calculate how long to wait */
		wait_usec =
			od_rate_duration_from_tokens(lim->limit, -remaining);

		uint32_t wait_ms = (uint32_t)(wait_usec / 1000);
		if (wait_ms == 0) {
			wait_ms = 1;
		}

		uint64_t remaining_ms = (end_us - now_us) / 1000;
		if (wait_ms >= remaining_ms) {
			return -1;
		}

		machine_sleep(wait_ms);
	}
}
