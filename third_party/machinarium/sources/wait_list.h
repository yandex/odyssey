#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#define MM_SLEEPY_NO_CORO_ID (~0ULL)

enum {
	MACHINE_WAIT_LIST_SUCCESS = 0,
	MACHINE_WAIT_LIST_ERR_TIMEOUT_OR_CANCEL = 1,
	MACHINE_WAIT_LIST_ERR_AGAIN = 2
};

typedef struct mm_sleepy {
	mm_event_t event;
	mm_list_t link;

	// we can store coroutine id and we will, in case of some debugging
	uint64_t coro_id;

	atomic_int released;
} mm_sleepy_t;

typedef struct mm_wait_list {
	mm_sleeplock_t lock;
	mm_list_t sleepies;
	atomic_uint_fast64_t sleepies_count;
	atomic_uint_fast64_t *word;
} mm_wait_list_t;

mm_wait_list_t *mm_wait_list_create(atomic_uint_fast64_t *word);
void mm_wait_list_destroy(mm_wait_list_t *wait_list);

int mm_wait_list_wait(mm_wait_list_t *wait_list, uint32_t timeout_ms);
int mm_wait_list_compare_wait(mm_wait_list_t *wait_list, uint64_t value,
			      uint32_t timeout_ms);
void mm_wait_list_notify(mm_wait_list_t *wait_list);
void mm_wait_list_notify_all(mm_wait_list_t *wait_list);
