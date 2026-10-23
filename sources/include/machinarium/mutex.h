#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium/sem.h>

typedef struct {
	mm_sem_t sem;
	void *owner_machine;
	uint64_t owner_coro_id;
} mm_mutex_t;

void mm_mutex_init(mm_mutex_t *mutex);
mm_mutex_t *mm_mutex_create(void);
void mm_mutex_free(mm_mutex_t *mutex);
void mm_mutex_destroy(mm_mutex_t *mutex);
/* returns 1 if mutex is locked, 0 otherwise */
int mm_mutex_lock(mm_mutex_t *mutex, uint32_t timeout_ms);
int mm_mutex_lock2(mm_mutex_t *mutex);
void mm_mutex_unlock(mm_mutex_t *mutex);
