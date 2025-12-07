#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <atomic.h>

#define OD_COUNTER_MAX_POSSIBLE_VALUE 1000UL

typedef struct od_counter {
	size_t size;
	od_atomic_u64_t value_to_count[FLEXIBLE_ARRAY_MEMBER];
} od_counter_t;

od_counter_t *od_counter_create(size_t max_value);
od_retcode_t od_counter_free(od_counter_t *counter);
uint64_t od_counter_get_count(od_counter_t *counter, size_t value);
od_retcode_t od_counter_reset(od_counter_t *counter, size_t value);
od_retcode_t od_counter_reset_all(od_counter_t *counter);
void od_counter_inc(od_counter_t *counter, size_t item);
