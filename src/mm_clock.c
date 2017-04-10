
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

static int
mm_timers_cmp(const void *a_ptr, const void *b_ptr)
{
	mm_timer_t *a = (mm_timer_t*)a_ptr;
	mm_timer_t *b = (mm_timer_t*)b_ptr;
	if (a->timeout < b->timeout)
		return 1;
	if (a->timeout > b->timeout)
		return 0;
	if (a->seq < b->seq)
		return 1;
	if (a->seq > b->seq)
		return 0;
	return 0;
}

void mm_clock_init(mm_clock_t *clock)
{
	mm_buf_init(&clock->timers);
	clock->timers_count = 0;
	clock->timers_seq = 0;
	clock->time = 0;
}

void mm_clock_free(mm_clock_t *clock)
{
	mm_buf_free(&clock->timers);
}

int mm_clock_timer_add(mm_clock_t *clock, mm_timer_t *timer)
{
	int count = clock->timers_count + 1;
	int rc;
	rc = mm_buf_ensure(&clock->timers, count * sizeof(mm_timer_t*));
	if (rc == -1)
		return -1;
	mm_timer_t **list = (mm_timer_t**)clock->timers.start;
	list[count - 1] = timer;
	mm_buf_advance(&clock->timers, sizeof(mm_timer_t*));
	timer->seq = clock->timers_seq++;
	timer->timeout = clock->time + timer->timeout;
	timer->active = 1;
	qsort(list, count, sizeof(mm_timer_t*),
	      mm_timers_cmp);
	clock->timers_count = count;
	return 0;
}

int mm_clock_timer_del(mm_clock_t *clock, mm_timer_t *timer)
{
	if (! timer->active)
		return -1;
	assert(clock->timers_count >= 1);
	mm_timer_t **list = (mm_timer_t**)clock->timers.start;
	int i = 0;
	int j = 0;
	for (; i < clock->timers_count; i++) {
		if (list[i] == timer)
			continue;
		list[j] = list[i];
		j++;
	}
	timer->active = 0;
	clock->timers.pos -= sizeof(mm_timer_t*);
	clock->timers_count -= 1;
	qsort(list, clock->timers_count, sizeof(mm_timer_t*),
	      mm_timers_cmp);
	return 0;
}

mm_timer_t*
mm_clock_timer_min(mm_clock_t *clock)
{
	if (clock->timers_count == 0)
		return NULL;
	mm_timer_t **list = (mm_timer_t**)clock->timers.start;
	return list[0];
}

int mm_clock_step(mm_clock_t *clock)
{
	if (clock->timers_count == 0)
		return 0;
	mm_timer_t **list = (mm_timer_t**)clock->timers.start;
	int timers_hit = 0;
	int i = 0;
	for (; i < clock->timers_count; i++) {
		mm_timer_t *timer = list[i];
		if (clock->time < timer->timeout)
			break;
		timer->callback(timer);
		timer->active = 0;
		timers_hit++;
		list[i] = NULL;
	}
	if (! timers_hit)
		return 0;
	int timers_left = clock->timers_count - timers_hit;
	if (timers_left == 0) {
		mm_buf_reset(&clock->timers);
		clock->timers_count = 0;
		return timers_hit;
	}
	memmove(clock->timers.start,
	        clock->timers.start + sizeof(mm_timer_t*) * timers_hit,
	        timers_left);
	clock->timers.pos -= sizeof(mm_timer_t*) * timers_hit;
	clock->timers_count -= timers_hit;
	return timers_hit;
}

static uint64_t
mm_clock_gettime(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * (uint64_t) 1e9 + t.tv_nsec;
}

void mm_clock_update(mm_clock_t *clock)
{
	/* set current time in millisecond precision */
	clock->time = mm_clock_gettime() / 1000000;
}
