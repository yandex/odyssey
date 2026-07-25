#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine
 *
 * yet another semaphore implementation for
 * machinaroum coroutines
 */

#include <stdatomic.h>

#include <machinarium/wait_list.h>

typedef struct {
	mm_wait_list_t notifier;
	atomic_uint_fast64_t value;
	uint64_t initial_value;
} mm_sem_t;

void mm_sem_init(mm_sem_t *sem, uint64_t value);
void mm_sem_destroy(mm_sem_t *sem);

int mm_sem_wait(mm_sem_t *sem);
int mm_sem_timedwait(mm_sem_t *sem, uint32_t timeout_ms);
void mm_sem_post(mm_sem_t *sem);

static inline uint64_t mm_sem_in_use(mm_sem_t *sem)
{
	uint64_t cur = atomic_load(&sem->value);
	/* cur can theoretically exceed initial_value during concurrent post */
	return cur <= sem->initial_value ? sem->initial_value - cur : 0;
}
