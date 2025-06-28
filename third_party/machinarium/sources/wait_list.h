#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#define MM_SLEEPY_NO_CORO_ID (~0ULL)

typedef struct mm_sleepy {
	mm_event_t event;
	machine_list_t link;

	/* we can store coroutine id and we will, in case of some debugging */
	uint64_t coro_id;

	int released;
} mm_sleepy_t;

typedef struct mm_wait_list {
	mm_sleeplock_t lock;
	machine_list_t sleepies;
	uint64_t sleepy_count;

	/*
	This field is analogous to a futex word.
	See futex(2) and futex(7) for details.
	*/
	atomic_uint_fast64_t *word;
} mm_wait_list_t;

mm_wait_list_t *mm_wait_list_create(atomic_uint_fast64_t *word);
void mm_wait_list_destroy(mm_wait_list_t *wait_list);

int mm_wait_list_wait(mm_wait_list_t *wait_list, uint32_t timeout_ms);
int mm_wait_list_compare_wait(mm_wait_list_t *wait_list, uint64_t value,
			      uint32_t timeout_ms);
void mm_wait_list_notify(mm_wait_list_t *wait_list);
void mm_wait_list_notify_all(mm_wait_list_t *wait_list);
