#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <stdatomic.h>

#include <machinarium/wait_list.h>

typedef struct mm_wait_flag {
	atomic_uint_fast64_t value;
	mm_wait_list_t *waiters;
	atomic_uint link_count;
} mm_wait_flag_t;

mm_wait_flag_t *mm_wait_flag_create(void);
void mm_wait_flag_destroy(mm_wait_flag_t *flag);
void mm_wait_flag_set(mm_wait_flag_t *flag);
int mm_wait_flag_wait(mm_wait_flag_t *flag, uint32_t timeout_ms);
