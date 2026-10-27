
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <assert.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include <machinarium/machinarium.h>
#include <machinarium/timer.h>
#include <machinarium/clock.h>

static inline int mm_clock_mask_test(const mm_clock_t *clock, int level, int slot)
{
	uint64_t mask = clock->wheel_mask[level][slot >> 6];
	return (mask & (1ULL << (slot & 63))) != 0;
}

static inline void mm_clock_mask_set(mm_clock_t *clock, int level, int slot)
{
	clock->wheel_mask[level][slot >> 6] |= 1ULL << (slot & 63);
}

static inline void mm_clock_mask_clear(mm_clock_t *clock, int level, int slot)
{
	clock->wheel_mask[level][slot >> 6] &= ~(1ULL << (slot & 63));
}

/*
 * Return the first set slot at or after "from", or -1.
 */
static int mm_clock_mask_first(const uint64_t *mask, int from)
{
	for (int word = from >> 6; word < MM_WHEEL_MASK_WORDS; word++) {
		uint64_t bits = mask[word];

		if (word == (from >> 6)) {
			bits &= ~0ULL << (from & 63);
		}
		if (bits != 0) {
			return word * 64 +
			       __builtin_ctzll((unsigned long long)bits);
		}
	}
	return -1;
}

static inline uint64_t mm_clock_bucket_start(const mm_timer_t *timer, int level)
{
	uint64_t step = 1ULL << (MM_WHEEL_SLOT_BITS * level);

	return timer->timeout & ~(step - 1);
}

/*
 * Pick a wheel level and a bucket for the timer. A bucket is allowed to
 * contain timers with the same bucket start only, so an occupied bucket
 * from another window forces the timer up to the next level. Returns -1
 * when the timer has to be put into the overflow list.
 */
static int mm_clock_link_level(mm_clock_t *clock, uint64_t timeout, uint64_t now, int *slot_out)
{
	uint64_t delta = timeout - now;
	int level = 0;

	while (level < MM_WHEEL_LEVELS - 1 &&
	       delta >= (1ULL << (MM_WHEEL_SLOT_BITS * (level + 1)))) {
		level++;
	}

	for (;;) {
		int shift = MM_WHEEL_SLOT_BITS * level;
		int window_shift = shift + MM_WHEEL_SLOT_BITS;
		uint64_t step = 1ULL << shift;
		uint64_t start = timeout & ~(step - 1);
		int slot;
		mm_list_t *bucket;

		if ((start >> window_shift) != (now >> window_shift)) {
			if (level == MM_WHEEL_LEVELS - 1) {
				return -1;
			}
			level++;
			continue;
		}

		slot = (int)((timeout >> shift) & MM_WHEEL_SLOT_MASK);
		bucket = &clock->wheel[level][slot];
		if (!mm_list_empty(bucket)) {
			mm_timer_t *head =
				mm_container_of(bucket->next, mm_timer_t, link);
			uint64_t head_start = head->timeout & ~(step - 1);

			if (head_start != start) {
				/*
				 * A bucket which is already due will be
				 * collected before any future timer is
				 * re-linked. It is safe to append to it: the
				 * two-phase collection sorts timers out after
				 * the bucket is emptied.
				 */
				if (head_start > now) {
					if (level == MM_WHEEL_LEVELS - 1) {
						return -1;
					}
					level++;
					continue;
				}
			}
		}

		*slot_out = slot;

		/*
		 * A future timer may only be placed into an already due
		 * bucket when that bucket is about to be collected and
		 * emptied by mm_clock_step().
		 */
		assert(start > now || !mm_list_empty(bucket));
		return level;
	}
}

static void mm_clock_link(mm_clock_t *clock, mm_timer_t *timer, uint64_t now)
{
	int level;
	int slot;

	if (timer->timeout <= now) {
		/*
		 * The timer is already due. Keep it in the current
		 * millisecond bucket; the next clock step will fire it.
		 */
		level = 0;
		slot = (int)(now & MM_WHEEL_SLOT_MASK);
	} else if (timer->timeout - now >= MM_WHEEL_MAX_TIMEOUT) {
		/*
		 * Do not let an out-of-range timeout wrap around the wheel
		 * mask and fire early. It is re-checked when the highest
		 * wheel makes a full revolution.
		 */
		timer->level = -1;
		timer->slot = 0;
		mm_list_append(&clock->overflow_list, &timer->link);
		return;
	} else {
		level = mm_clock_link_level(clock, timer->timeout, now, &slot);
		if (level < 0) {
			timer->level = -1;
			timer->slot = 0;
			mm_list_append(&clock->overflow_list, &timer->link);
			return;
		}
	}

	timer->level = level;
	timer->slot = slot;
	mm_list_append(&clock->wheel[level][slot], &timer->link);
	mm_clock_mask_set(clock, level, slot);
}

static void mm_clock_collect_bucket(mm_clock_t *clock, int level, int slot,
				    mm_list_t *pending)
{
	mm_list_t *bucket = &clock->wheel[level][slot];
	mm_list_t *first;
	mm_list_t *last;

	if (mm_list_empty(bucket)) {
		return;
	}

	first = bucket->next;
	last = bucket->prev;

	first->prev = pending->prev;
	pending->prev->next = first;
	last->next = pending;
	pending->prev = last;

	mm_list_init(bucket);
	mm_clock_mask_clear(clock, level, slot);
}

static int mm_clock_fire_pending(mm_clock_t *clock, mm_list_t *pending,
				 uint64_t now)
{
	mm_list_t *i = pending->next;
	int fired = 0;

	while (i != pending) {
		mm_timer_t *timer = mm_container_of(i, mm_timer_t, link);

		i = i->next;

		/* The callback of a previous timer could stop this one. */
		if (!timer->active) {
			continue;
		}

		mm_list_unlink(&timer->link);
		if (timer->timeout <= now) {
			timer->active = 0;
			timer->level = -1;
			clock->timers_count--;
			timer->callback(timer);
			fired++;
		} else {
			mm_clock_link(clock, timer, now);
		}
	}

	return fired;
}

static void mm_clock_collect_level(mm_clock_t *clock, int level, uint64_t last,
				   uint64_t now, mm_list_t *pending)
{
	int shift = MM_WHEEL_SLOT_BITS * level;
	int last_slot = (int)((last >> shift) & MM_WHEEL_SLOT_MASK);
	int now_slot = (int)((now >> shift) & MM_WHEEL_SLOT_MASK);
	uint64_t last_window = last >> (shift + MM_WHEEL_SLOT_BITS);
	uint64_t now_window = now >> (shift + MM_WHEEL_SLOT_BITS);

	if (last_window != now_window) {
		/*
		 * The level window has changed. Every bucket that belongs to
		 * an older window is due now; a bucket from the new window
		 * can be due as well. Check the actual bucket start to skip
		 * timers that are still in the future.
		 */
		for (int slot = 0; slot < MM_WHEEL_SLOTS; slot++) {
			mm_list_t *bucket = &clock->wheel[level][slot];
			mm_timer_t *timer;
			uint64_t start;

			if (mm_list_empty(bucket)) {
				continue;
			}
			timer = mm_container_of(bucket->next, mm_timer_t, link);
			start = mm_clock_bucket_start(timer, level);
			if (start > now) {
				continue;
			}
			mm_clock_collect_bucket(clock, level, slot, pending);
		}
		return;
	}

	/*
	 * Level zero also includes the last processed slot: a timer with
	 * zero interval can be added into the current millisecond after the
	 * previous step.
	 */
	int first = level == 0 ? last_slot : last_slot + 1;
	int end = now_slot;

	if (first > end) {
		return;
	}

	for (int slot = first; slot <= end; slot++) {
		if (!mm_clock_mask_test(clock, level, slot)) {
			continue;
		}
		mm_clock_collect_bucket(clock, level, slot, pending);
	}
}

static int mm_clock_advance_overflow(mm_clock_t *clock, uint64_t now)
{
	mm_list_t pending;
	mm_list_t *i;
	int fired = 0;

	if (mm_list_empty(&clock->overflow_list)) {
		return 0;
	}

	/*
	 * Splice the overflow list into a local list, so a timer that is
	 * re-linked back into overflow is not visited twice.
	 */
	mm_list_init(&pending);
	while (!mm_list_empty(&clock->overflow_list)) {
		mm_list_t *node = clock->overflow_list.next;

		mm_list_unlink(node);
		mm_list_append(&pending, node);
	}

	i = pending.next;
	while (i != &pending) {
		mm_timer_t *timer = mm_container_of(i, mm_timer_t, link);

		i = i->next;

		if (!timer->active) {
			continue;
		}

		if (timer->timeout <= now) {
			mm_list_unlink(&timer->link);
			timer->active = 0;
			timer->level = -1;
			clock->timers_count--;
			timer->callback(timer);
			fired++;
		} else if (timer->timeout - now < MM_WHEEL_MAX_TIMEOUT) {
			mm_list_unlink(&timer->link);
			mm_clock_link(clock, timer, now);
		}
	}

	/* Return timers that still do not fit into the wheel. */
	while (!mm_list_empty(&pending)) {
		mm_list_t *node = pending.next;

		mm_list_unlink(node);
		mm_list_append(&clock->overflow_list, node);
	}

	return fired;
}

void mm_clock_init(mm_clock_t *clock)
{
	for (int level = 0; level < MM_WHEEL_LEVELS; level++) {
		for (int slot = 0; slot < MM_WHEEL_SLOTS; slot++) {
			mm_list_init(&clock->wheel[level][slot]);
		}
	}
	memset(clock->wheel_mask, 0, sizeof(clock->wheel_mask));
	mm_list_init(&clock->overflow_list);
	clock->wheel_last_ms = 0;
	clock->timers_count = 0;
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
	uint64_t now = clock->time_ms;

	timer->seq = clock->timers_seq++;
	if (timer->interval > UINT64_MAX - now) {
		/* Do not wrap the deadline around and fire it early. */
		timer->timeout = UINT64_MAX;
	} else {
		timer->timeout = now + timer->interval;
	}
	timer->active = 1;
	timer->clock = clock;

	clock->timers_count++;
	mm_clock_link(clock, timer, now);

	return 0;
}

int mm_clock_timer_del(mm_clock_t *clock, mm_timer_t *timer)
{
	if (!timer->active) {
		return -1;
	}

	mm_list_unlink(&timer->link);

	if (timer->level >= 0) {
		mm_list_t *bucket = &clock->wheel[timer->level][timer->slot];

		if (mm_list_empty(bucket)) {
			mm_clock_mask_clear(clock, timer->level, timer->slot);
		}
	}

	timer->active = 0;
	timer->level = -1;
	clock->timers_count--;

	return 0;
}

int mm_clock_step(mm_clock_t *clock)
{
	uint64_t now = clock->time_ms;
	uint64_t last = clock->wheel_last_ms;
	int total_fired = 0;
	int fired = 0;

	if (clock->timers_count == 0) {
		clock->wheel_last_ms = now;
		clock->active = 0;
		return 0;
	}

	do {
		mm_list_t pending;

		mm_list_init(&pending);

		for (int level = MM_WHEEL_LEVELS - 1; level > 0; level--) {
			mm_clock_collect_level(clock, level, last, now,
					       &pending);
		}
		mm_clock_collect_level(clock, 0, last, now, &pending);

		fired = mm_clock_fire_pending(clock, &pending, now);

		if ((last >> MM_WHEEL_TIMEOUT_BITS) !=
		    (now >> MM_WHEEL_TIMEOUT_BITS)) {
			fired += mm_clock_advance_overflow(clock, now);
		}

		clock->wheel_last_ms = now;
		last = now;
		total_fired += fired;

		/*
		 * A callback may have added a timer which is already due.
		 * Run another pass on the updated wheel.
		 */
	} while (fired > 0);

	if (clock->timers_count == 0) {
		clock->active = 0;
	}

	return total_fired;
}

uint64_t mm_clock_next_timeout(mm_clock_t *clock)
{
	uint64_t now = clock->time_ms;
	uint64_t next = UINT64_MAX;

	if (clock->timers_count == 0) {
		return UINT64_MAX;
	}

	for (int level = 0; level < MM_WHEEL_LEVELS; level++) {
		int shift = MM_WHEEL_SLOT_BITS * level;
		int now_slot = (int)((now >> shift) & MM_WHEEL_SLOT_MASK);
		int slot = mm_clock_mask_first(clock->wheel_mask[level],
					       now_slot + 1);
		uint64_t window;
		uint64_t start;

		if (slot < 0) {
			continue;
		}

		window = 1ULL << (shift + MM_WHEEL_SLOT_BITS);
		start = (now & ~(window - 1)) + ((uint64_t)slot << shift);
		if (start < next) {
			next = start;
		}
	}

	if (!mm_list_empty(&clock->overflow_list)) {
		uint64_t next_window = ((now >> MM_WHEEL_TIMEOUT_BITS) + 1)
				       << MM_WHEEL_TIMEOUT_BITS;

		if (next_window < next) {
			next = next_window;
		}
	}

	return next;
}

static uint64_t mm_clock_gettime(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * (uint64_t)1e9 + t.tv_nsec;
}

static int64_t mm_clock_gettimeofday(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (int64_t)tv.tv_sec;
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
