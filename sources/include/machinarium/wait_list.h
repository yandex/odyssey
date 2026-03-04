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

#include <stdint.h>

#include <machinarium/sleep_lock.h>
#include <machinarium/event.h>
#include <machinarium/list.h>

#define MM_SLEEPY_NO_CORO_ID (~0ULL)

typedef struct mm_sleepy {
	mm_event_t event;
	mm_list_t link;

	/* we can store coroutine id and we will, in case of some debugging */
	uint64_t coro_id;

	int released;
} mm_sleepy_t;

typedef struct {
	mm_sleeplock_t lock;
	mm_list_t sleepies;
	uint64_t sleepy_count;

	/*
	This field is analogous to a futex word.
	See futex(2) and futex(7) for details.
	*/
	atomic_uint_fast64_t *word;
} mm_wait_list_t;

/*
 * The `word` argument in create(...) is analogous to a futex word.
 * Pass NULL if compare_wait functionality isn't needed.
 */
void mm_wait_list_init(mm_wait_list_t *wait_list, atomic_uint_fast64_t *word);
mm_wait_list_t *mm_wait_list_create(atomic_uint_fast64_t *word);

/*
 * If destroy() is called before all notify() and wait() calls are completed, the behaviour is undefined.
 * Additional synchronization (e.g., using a wait group) may be required to ensure that.
*/
void mm_wait_list_destroy(mm_wait_list_t *wait_list);
void mm_wait_list_free(mm_wait_list_t *wait_list);

int mm_wait_list_wait(mm_wait_list_t *wait_list, uint32_t timeout_ms);
int mm_wait_list_compare_wait(mm_wait_list_t *wait_list, uint64_t value,
			      uint32_t timeout_ms);
void mm_wait_list_notify(mm_wait_list_t *wait_list);
void mm_wait_list_notify_all(mm_wait_list_t *wait_list);
