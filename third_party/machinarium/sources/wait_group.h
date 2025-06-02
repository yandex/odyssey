#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

typedef struct mm_wait_group {
	uint64_t counter;
	mm_wait_list_t *waiters;
} mm_wait_group_t;

mm_wait_group_t *mm_wait_group_create();
void mm_wait_group_destroy(mm_wait_group_t *group);
void mm_wait_group_add(mm_wait_group_t *group);
uint64_t mm_wait_group_count(mm_wait_group_t *group);
void mm_wait_group_done(mm_wait_group_t *group);
int mm_wait_group_wait(mm_wait_group_t *group, uint32_t timeout_ms);
