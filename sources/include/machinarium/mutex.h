#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <assert.h>

#include <machinarium/sem.h>

typedef struct {
	mm_sem_t sem;
	void *owner_machine;
	uint64_t owner_coro_id;
} mm_mutex_t;

#define MM_MUTEX_PADDED_SIZE 128

typedef union {
	mm_mutex_t mutex;
	char pad[MM_MUTEX_PADDED_SIZE];
} mm_mutex_padded_t;

static_assert(sizeof(mm_mutex_t) <= MM_MUTEX_PADDED_SIZE,
	      "mutex must fit into a padded mutex slot");
static_assert(sizeof(mm_mutex_padded_t) == MM_MUTEX_PADDED_SIZE,
	      "padded mutex must occupy whole cache lines");

static inline mm_mutex_t *mm_mutex_padded_at(mm_mutex_padded_t *array, size_t i)
{
	return &array[i].mutex;
}

void mm_mutex_init(mm_mutex_t *mutex);
mm_mutex_t *mm_mutex_create(void);
void mm_mutex_free(mm_mutex_t *mutex);
void mm_mutex_destroy(mm_mutex_t *mutex);
/* returns 1 if mutex is locked, 0 otherwise */
int mm_mutex_lock(mm_mutex_t *mutex, uint32_t timeout_ms);
int mm_mutex_lock2(mm_mutex_t *mutex);
void mm_mutex_unlock(mm_mutex_t *mutex);
