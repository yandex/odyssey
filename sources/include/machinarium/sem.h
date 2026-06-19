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
} mm_sem_t;

void mm_sem_init(mm_sem_t *sem, uint64_t value);
void mm_sem_destroy(mm_sem_t *sem);

int mm_sem_wait(mm_sem_t *sem);
void mm_sem_post(mm_sem_t *sem);
