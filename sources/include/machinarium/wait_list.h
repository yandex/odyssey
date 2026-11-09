#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

/*
 * A wait list is a structure that is similar to a futex (see futex(2) and futex(7) for details).
 * It allows a coroutine to wait until a specific condition is met.
 * Wait lists can be shared among different workers and are suitable for implementing other thread synchronization primitives.
 *
 * If compare-and-wait functionality is not needed, you can pass NULL when creating a wait list and simply use the wait(...) method.
 * However, this may result in lost wake-ups, so do it only if acceptable.
 *
 * Local progress is guaranteed (no coroutine starvation) but a FIFO ordering is not.
 * Spurious wake-ups are possible.
 */

#include <stdatomic.h>
#include <stdint.h>

#include <machinarium/spinlock.h>
#include <machinarium/event.h>
#include <machinarium/list.h>

#define MM_SLEEPY_NO_CORO_ID (~0ULL)

typedef enum { EXCLUSIVE, SHARED } mm_sleepy_type_t;

typedef struct mm_sleepy {
	union {
		mm_event_t event;
		mm_event_t *shared_event;
	};
	mm_sleepy_type_t type;

	mm_list_t link;

	/* some additional info about sleepy, to return it from notify() */
	void *private;

	/* we can store coroutine id and we will, in case of some debugging */
	uint64_t coro_id;

	int released;

	uint64_t seq;
	int shard;
} mm_sleepy_t;

/*
 * A single FIFO queue of sleepies.  A wait list consists of one or more
 * shards; a waiter is enqueued into the shard of its machine
 * (mm_self->id % n_shards), so a waker running on the same machine can
 * find and wake a same-machine waiter in O(1) without scanning the
 * whole queue.
 *
 * All wait list fields (counters, sequence, bitmap, shard heads) are
 * mutated under the single wait list lock, so tightly packed shard
 * heads do not introduce cross-thread cache line ping-pong beyond the
 * lock line itself.  If the lock is ever sharded, the shard heads must
 * be spread across cache lines (padded, see the mm_mutex_padded_t
 * precedent in ds/hm.c).
 *
 * The wait list struct must stay compact: it is embedded into
 * mm_sem_t/mm_mutex_t which are bounded by MM_MUTEX_PADDED_SIZE.
 */
typedef struct mm_wl_shard {
	mm_list_t head;
} mm_wl_shard_t;

/* upper bound of shards per wait list, see mm_wait_list_init_sharded() */
#define MM_WL_MAX_SHARDS 512

typedef struct {
	mm_spinlock_t lock;
	atomic_uint_fast64_t sleepy_count;

	/*
	This field is analogous to a futex word.
	See futex(2) and futex(7) for details.
	*/
	atomic_uint_fast64_t *word;

	uint64_t seq;

	mm_wl_shard_t base;
	mm_wl_shard_t *shards;
	int n_shards;

	/*
	 * bitmap of shards with non-empty queues, maintained under the
	 * wait list lock; lets the global-oldest scan skip empty shards
	 * without touching their cache lines.  Owned together with the
	 * shards array by the user of the wait list (one aligned
	 * allocation keeps all); NULL when the wait list is not sharded.
	 */
	uint64_t *nonempty;

	/*
	 * per-shard service counters, in the same owned allocation as
	 * the bitmap and shards; NULL when the wait list is not sharded.
	 * Used by the affinity wakeup to bound the service lag between
	 * shards (anti-starvation).
	 */
	uint64_t *served;
} mm_wait_list_t;

/*
 * Affinity wakeup serves the caller's own shard only while its service
 * counter runs at most this many serves ahead of the most lagging
 * non-empty foreign shard; past that the globally oldest waiter is
 * served instead, so no shard can be starved of wakeups.
 */
#define MM_WL_SERVICE_LAG 4

/*
 * The `word` argument in create(...) is analogous to a futex word.
 * Pass NULL if compare_wait functionality isn't needed.
 */
void mm_wait_list_init(mm_wait_list_t *wait_list, atomic_uint_fast64_t *word);
mm_wait_list_t *mm_wait_list_create(atomic_uint_fast64_t *word);

/*
 * Enable sharding with externally owned shards array, non-empty bitmap
 * and per-shard service counters (all must remain valid for the
 * lifetime of the wait list, no allocation is done here; they are
 * commonly placed into one aligned allocation).
 * Waiters are placed into shards[mm_self->id % n_shards]; n_shards must
 * cover the number of distinct machines to keep one machine per shard,
 * the caller machine ids are expected to form a consecutive block
 * (see machine_reserve_ids()).  n_shards must not exceed
 * MM_WL_MAX_SHARDS.
 */
void mm_wait_list_init_sharded(mm_wait_list_t *wait_list,
			       atomic_uint_fast64_t *word, uint64_t *nonempty,
			       uint64_t *served, mm_wl_shard_t *shards,
			       int n_shards);

/*
 * If destroy() is called before all notify() and wait() calls are completed, the behaviour is undefined.
 * Additional synchronization (e.g., using a wait group) may be required to ensure that.
*/
void mm_wait_list_destroy(mm_wait_list_t *wait_list);
void mm_wait_list_free(mm_wait_list_t *wait_list);

int mm_wait_list_wait(mm_wait_list_t *wait_list, void *private,
		      uint32_t timeout_ms);
int mm_wait_list_compare_wait(mm_wait_list_t *wait_list, void *private,
			      uint64_t value, uint32_t timeout_ms);

/* returns private if someones woke, NULL otherwise */
void *mm_wait_list_notify(mm_wait_list_t *wait_list);
int mm_wait_list_notify_all(mm_wait_list_t *wait_list);

/* calls cb before signalling the sleepy */
typedef void (*mm_wl_private_cb_t)(void *private, void *arg);
void *mm_wait_list_notify_cb(mm_wait_list_t *wait_list, mm_wl_private_cb_t cb,
			     void *arg);

/*
 * Same as mm_wait_list_notify_cb(), but a waiter from the caller's own
 * machine shard is preferred over the globally oldest one.  If the own
 * shard is empty, the globally oldest waiter is woken (global FIFO
 * fallback).  Useful for cross-machine wait lists where waking a
 * same-machine waiter avoids the cross-machine wakeup latency.
 */
void *mm_wait_list_notify_cb_affinity(mm_wait_list_t *wait_list,
				      mm_wl_private_cb_t cb, void *arg);

/*
 * Wait for any of provided wait_lists to trigger.
 * On success returns 0
 * On timeout or error returns -1
 */
int mm_wait_list_waitv(mm_wait_list_t **wait_lists, size_t count,
		       uint32_t timeout_ms);

/*
 * Cheap racy check, to be used as a fast path right after the futex word
 * was changed by the caller (e.g. mutex unlock):
 *
 * - a non-zero result does not guarantee the list is non-empty, notify()
 *   will then find nothing and return, which is cheap;
 * - a zero result does not guarantee the list is empty, but a waiter that
 *   is being queued concurrently is safe: mm_wait_list_compare_wait()
 *   re-checks the word after enqueueing and bails out instead of sleeping.
 *
 * Waiters queued via mm_wait_list_wait() do not re-check anything, so
 * using this hint with plain wait() may lose wakeups.
 */
static inline int mm_wait_list_maybe_waiters(mm_wait_list_t *wait_list)
{
	return atomic_load(&wait_list->sleepy_count) != 0;
}
