#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <stdint.h>

#include <machinarium/list.h>
#include <machinarium/timer.h>

/*
 * Hierarchical timer wheel.
 *
 * Four levels of 256 slots with 1ms resolution cover 2^32 ms (~49.7 days).
 * A timer is placed into the smallest level that can hold it without
 * wrapping around the current level window. Timers that do not fit into the
 * wheel (including intervals of 2^32 ms and longer) are kept in
 * overflow_list and re-checked when the highest wheel makes a full
 * revolution.
 */
#define MM_WHEEL_LEVELS 4
#define MM_WHEEL_SLOTS 256
#define MM_WHEEL_SLOT_BITS 8
#define MM_WHEEL_SLOT_MASK (MM_WHEEL_SLOTS - 1)
#define MM_WHEEL_MASK_WORDS (MM_WHEEL_SLOTS / 64)
#define MM_WHEEL_TIMEOUT_BITS (MM_WHEEL_SLOT_BITS * MM_WHEEL_LEVELS)
#define MM_WHEEL_MAX_TIMEOUT (1ULL << MM_WHEEL_TIMEOUT_BITS)

typedef struct mm_clock mm_clock_t;

struct mm_clock {
	int active;
	int time_cached;
	uint64_t time_ms;
	uint64_t time_us;
	uint64_t time_ns;
	int64_t time_sec;

	mm_list_t wheel[MM_WHEEL_LEVELS][MM_WHEEL_SLOTS];
	uint64_t wheel_mask[MM_WHEEL_LEVELS][MM_WHEEL_MASK_WORDS];
	mm_list_t overflow_list;
	uint64_t wheel_last_ms;
	uint64_t timers_count;
	uint32_t timers_seq;
};

void mm_clock_init(mm_clock_t *);
void mm_clock_free(mm_clock_t *);
void mm_clock_update(mm_clock_t *);
int mm_clock_step(mm_clock_t *);
int mm_clock_timer_add(mm_clock_t *, mm_timer_t *);
int mm_clock_timer_del(mm_clock_t *, mm_timer_t *);

uint64_t mm_clock_next_timeout(mm_clock_t *);

static inline void mm_clock_reset(mm_clock_t *clock)
{
	clock->time_cached = 0;
}

static inline void mm_timer_start(mm_clock_t *clock, mm_timer_t *timer)
{
	if (!clock->active) {
		mm_clock_update(clock);
		clock->wheel_last_ms = clock->time_ms;
	}
	clock->active = 1;
	mm_clock_timer_add(clock, timer);
}

static inline void mm_timer_stop(mm_timer_t *timer)
{
	if (!timer->active) {
		return;
	}
	mm_clock_t *clock = timer->clock;
	mm_clock_timer_del(clock, timer);
	if (clock->timers_count == 0) {
		clock->active = 0;
	}
}
