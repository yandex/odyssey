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
od_rate_limiter_t *od_rate_limiter_create(uint64_t limit)
{
	od_rate_limiter_t *lim = od_malloc(sizeof(od_rate_limiter_t));
	if (lim == NULL) {
		return NULL;
	}

	lim->limit = limit;
	/* Fill just-created limiter with single token */
	lim->tokens = RATE_TOKEN_SCALE;
	lim->last = machine_time_us();
	lim->waiters = mm_wait_list_create(NULL);
	if (lim->waiters == NULL) {
		od_free(lim);
		return NULL;
	}
	mm_sleeplock_init(&lim->lock);

	return lim;
}

void od_rate_limiter_destroy(od_rate_limiter_t *lim)
{
	od_assert(lim);
	mm_wait_list_free(lim->waiters);
}

void od_rate_limiter_free(od_rate_limiter_t *lim)
{
	od_assert(lim);
	od_rate_limiter_destroy(lim);
	od_free(lim);
}

/* NB: all current users call this with n == 1. */
int od_rate_limiter_waitn(od_rate_limiter_t *lim, uint64_t n)
{
	od_assert(lim);
	od_assert(lim->limit);

	int64_t now;
	int64_t wait_usec = 0;

	for (;;) {
		/* XXX: check for cancellation here ? */

		/* refresh and go */
		now = machine_time_us();

		mm_sleeplock_lock(&lim->lock);

		int64_t tokens = od_rate_advance(lim, now);

		/* consume n tokens */
		int64_t remaining = tokens - (int64_t)(n * RATE_TOKEN_SCALE);

		if (remaining >= 0) {
			lim->tokens = remaining;
			mm_sleeplock_unlock(&lim->lock);
			return 0;
		}
		mm_sleeplock_unlock(&lim->lock);

		/* not enough tokens: calculate how long to wait */
		wait_usec =
			od_rate_duration_from_tokens(lim->limit, -remaining);

		uint32_t wait_ms = (uint32_t)(wait_usec / 1000);
		if (wait_ms == 0) {
			wait_ms = 1;
		}

		(void)mm_wait_list_wait(lim->waiters, NULL, wait_ms);
	}
}
