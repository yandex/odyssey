#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <stdatomic.h>

#include <machinarium/machine.h>
#include <machinarium/coroutine.h>
#include <machinarium/event.h>
#include <machinarium/sleep_lock.h>

typedef struct {
	mm_machine_t *machine;
	mm_coroutine_t *coroutine;

	mm_event_t event;
	mm_list_t link;
} mm_mutex_owner_t;

typedef struct {
	atomic_int state;
	mm_mutex_owner_t owner;

	mm_list_t queue;
	atomic_uint_fast64_t queue_size;
	mm_sleeplock_t queue_lock;
} mm_mutex_t;

mm_mutex_t *mm_mutex_create();
void mm_mutex_destroy(mm_mutex_t *mutex);
int mm_mutex_lock(mm_mutex_t *mutex, uint32_t timeout_ms);
void mm_mutex_unlock(mm_mutex_t *mutex);
