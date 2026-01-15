#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <stdatomic.h>

#include <machinarium/wait_list.h>

typedef struct mm_wait_group {
	atomic_uint_fast64_t counter;
	mm_wait_list_t *waiters;
	atomic_uint link_count;
} mm_wait_group_t;

mm_wait_group_t *mm_wait_group_create(void);
void mm_wait_group_destroy(mm_wait_group_t *group);
void mm_wait_group_add(mm_wait_group_t *group);
uint64_t mm_wait_group_count(mm_wait_group_t *group);
void mm_wait_group_done(mm_wait_group_t *group);
int mm_wait_group_wait(mm_wait_group_t *group, uint32_t timeout_ms);
