#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <stdatomic.h>

#include <machinarium/wait_list.h>

typedef struct {
	atomic_uint_fast64_t state;
	atomic_uintptr_t owner_machine;
	atomic_uint_fast64_t owner_coro_id;
	mm_wait_list_t *wl;
} mm_mutex_t;

mm_mutex_t *mm_mutex_create(void);
void mm_mutex_destroy(mm_mutex_t *mutex);
int mm_mutex_lock(mm_mutex_t *mutex, uint32_t timeout_ms);
void mm_mutex_unlock(mm_mutex_t *mutex);
