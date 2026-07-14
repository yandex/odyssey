
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <time.h>
#include <sys/time.h>

#include <machinarium/machinarium.h>
#include <machinarium/timer.h>
#include <machinarium/clock.h>

static int mm_clock_cmp(const mm_heap_node_t *aa, const mm_heap_node_t *bb)
{
	const mm_timer_t *a = mm_container_of(aa, mm_timer_t, heap);
	const mm_timer_t *b = mm_container_of(bb, mm_timer_t, heap);

	if (a->timeout < b->timeout) {
		return 1;
	}

	if (b->timeout < a->timeout) {
		return 0;
	}

	return a->seq < b->seq;
}

void mm_clock_init(mm_clock_t *clock)
{
	mm_heap_init(&clock->timers, mm_clock_cmp);
	clock->timers_seq = 0;
	clock->active = 0;
	clock->time_ms = 0;
	clock->time_ns = 0;
	clock->time_us = 0;
	clock->time_sec = 0;
	clock->time_cached = 0;
}

void mm_clock_free(mm_clock_t *clock)
{
	(void)clock;
}

int mm_clock_timer_add(mm_clock_t *clock, mm_timer_t *timer)
{
	timer->seq = clock->timers_seq++;
	timer->timeout = clock->time_ms + timer->interval;
	timer->active = 1;
	timer->clock = clock;

	mm_heap_push(&clock->timers, &timer->heap);

	return 0;
}

int mm_clock_timer_del(mm_clock_t *clock, mm_timer_t *timer)
{
	if (!timer->active) {
		return -1;
	}

	mm_heap_remove(&clock->timers, &timer->heap);

	timer->active = 0;

	return 0;
}

mm_timer_t *mm_clock_timer_min(mm_clock_t *clock)
{
	mm_heap_node_t *m = mm_heap_min(&clock->timers);
	if (m != NULL) {
		return mm_container_of(m, mm_timer_t, heap);
	}

	return NULL;
}

void mm_clock_step(mm_clock_t *clock)
{
	while (1) {
		mm_heap_node_t *n = mm_heap_min(&clock->timers);
		if (n == NULL) {
			break;
		}

		mm_timer_t *tm = mm_container_of(n, mm_timer_t, heap);
		if (tm->timeout > clock->time_ms) {
			break;
		}

		mm_heap_pop(&clock->timers);
		tm->active = 0;
		tm->callback(tm);
	}
}

static uint64_t mm_clock_gettime(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * (uint64_t)1e9 + t.tv_nsec;
}

static uint32_t mm_clock_gettimeofday(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec;
}

void mm_clock_update(mm_clock_t *clock)
{
	if (clock->time_cached) {
		return;
	}
	clock->time_ns = mm_clock_gettime();
	clock->time_ms = clock->time_ns / 1000000;
	clock->time_us = clock->time_ns / 1000;
	clock->time_sec = mm_clock_gettimeofday();
	clock->time_cached = 1;
}
