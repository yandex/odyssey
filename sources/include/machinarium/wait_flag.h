#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <stdatomic.h>

#include <machinarium/wait_list.h>

/*
 * one-shot synchronization privimive
 *
 * can be used from multiple workers,
 * both set and wait functions can be called multiple times.
*/

typedef struct mm_wait_flag {
	atomic_uint_fast64_t value;
	mm_wait_list_t waiters;
	atomic_uint link_count;
} mm_wait_flag_t;

void mm_wait_flag_init(mm_wait_flag_t *flag);
void mm_wait_flag_destroy(mm_wait_flag_t *flag);
void mm_wait_flag_set(mm_wait_flag_t *flag);
int mm_wait_flag_wait(mm_wait_flag_t *flag, uint32_t timeout_ms);
