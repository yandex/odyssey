
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

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
#include <intrin.h>
#endif

static inline int mm_clock_ctz64(uint64_t bits)
{
	assert(bits != 0);

#if defined(__GNUC__) || defined(__clang__)
	return __builtin_ctzll(bits);
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
	unsigned long index;

	_BitScanForward64(&index, bits);
	return (int)index;
#else
	int index = 0;

	while ((bits & 1) == 0) {
		bits >>= 1;
		index++;
	}
	return index;
#endif
}

static inline int mm_clock_mask_test(const mm_clock_t *clock, int level, int slot)
{
	uint64_t mask = clock->wheel_mask[level][slot >> MM_WHEEL_MASK_SHIFT];

	return (mask & (1ULL << (slot & MM_WHEEL_MASK_BIT_MASK))) != 0;
}

static inline void mm_clock_mask_set(mm_clock_t *clock, int level, int slot)
{
	clock->wheel_mask[level][slot >> MM_WHEEL_MASK_SHIFT] |=
		1ULL << (slot & MM_WHEEL_MASK_BIT_MASK);
}

static inline void mm_clock_mask_clear(mm_clock_t *clock, int level, int slot)
{
	clock->wheel_mask[level][slot >> MM_WHEEL_MASK_SHIFT] &=
		~(1ULL << (slot & MM_WHEEL_MASK_BIT_MASK));
}

/*
 * Return the first set slot at or after "from", or -1.
 */
static int mm_clock_mask_first(const uint64_t *mask, int from)
{
	int first_word = from >> MM_WHEEL_MASK_SHIFT;

	for (int word = first_word; word < MM_WHEEL_MASK_WORDS; word++) {
		uint64_t bits = mask[word];

		if (word == first_word) {
			bits &= ~0ULL << (from & MM_WHEEL_MASK_BIT_MASK);
		}
		if (bits != 0) {
			return word * MM_WHEEL_MASK_BITS + mm_clock_ctz64(bits);
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
 * Pick a wheel level and a bucket for a future timer. The deadline may
 * cross the current window of the selected level, in which case the timer
 * is promoted to the next level. Returns -1 when the timer does not fit
 * into the wheel and has to be put into the overflow list.
 */
static int mm_clock_link_level(uint64_t timeout, uint64_t now, int *slot_out)
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

		if ((start >> window_shift) != (now >> window_shift)) {
			if (level == MM_WHEEL_LEVELS - 1) {
				return -1;
			}
			level++;
			continue;
		}

		slot = (int)((timeout >> shift) & MM_WHEEL_SLOT_MASK);
		*slot_out = slot;

		assert(start > now);
		return level;
	}
}

static void mm_clock_link(mm_clock_t *clock, mm_timer_t *timer, uint64_t now)
{
	int level = -1;
	int slot = 0;

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
		level = mm_clock_link_level(timer->timeout, now, &slot);
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
	int fired = 0;

	/*
	 * Pop the timer before its callback. A callback may stop and
	 * restart another timer from the same pending list; without the
	 * pop, a saved "next" pointer could point into the wheel after
	 * such a restart. Popping keeps the pending list correct because
	 * mm_clock_timer_del() unlinks a stopped timer by itself.
	 */
	while (!mm_list_empty(pending)) {
		mm_timer_t *timer =
			mm_container_of(mm_list_pop(pending), mm_timer_t, link);

		assert(timer->active);

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
		for (int word = 0; word < MM_WHEEL_MASK_WORDS; word++) {
			uint64_t bits = clock->wheel_mask[level][word];

			while (bits != 0) {
				int bit = mm_clock_ctz64(bits);
				int slot = word * MM_WHEEL_MASK_BITS + bit;
				mm_list_t *bucket = &clock->wheel[level][slot];
				mm_timer_t *timer;
				uint64_t start;

				bits &= bits - 1;
				if (mm_list_empty(bucket)) {
					mm_clock_mask_clear(clock, level, slot);
					continue;
				}
				timer = mm_container_of(bucket->next,
							mm_timer_t, link);
				start = mm_clock_bucket_start(timer, level);
				if (start > now) {
					continue;
				}
				mm_clock_collect_bucket(clock, level, slot,
							pending);
			}
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
	int fired = 0;

	if (mm_list_empty(&clock->overflow_list)) {
		return 0;
	}

	/*
	 * Splice the overflow list into a local list. Then pop timers one
	 * by one: a callback may stop and restart another overflow timer,
	 * and a saved "next" pointer would become invalid.
	 */
	mm_list_init(&pending);
	while (!mm_list_empty(&clock->overflow_list)) {
		mm_list_t *node = clock->overflow_list.next;

		mm_list_unlink(node);
		mm_list_append(&pending, node);
	}

	while (!mm_list_empty(&pending)) {
		mm_timer_t *timer = mm_container_of(mm_list_pop(&pending),
						    mm_timer_t, link);

		assert(timer->active);

		if (timer->timeout <= now) {
			timer->active = 0;
			timer->level = -1;
			clock->timers_count--;
			timer->callback(timer);
			fired++;
		} else if (timer->timeout - now < MM_WHEEL_MAX_TIMEOUT) {
			mm_clock_link(clock, timer, now);
		} else {
			mm_list_append(&clock->overflow_list, &timer->link);
		}
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

		/*
		 * Due buckets are collected from the highest level to the
		 * lowest one. Timers with the same deadline keep FIFO order,
		 * but timers with different deadlines that expire within the
		 * same clock step are not guaranteed to fire in deadline
		 * order. This is acceptable for timer callbacks which only
		 * wake coroutines, but should be kept in mind by future
		 * callbacks.
		 */
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

/*
 * Return the absolute lower bound of the next timer deadline. Level zero
 * returns the exact millisecond, while higher levels return the start of
 * the next bucket. A bucket may start before its earliest timer, so the
 * event loop can wake up a few times while a long timer is cascaded down.
 * Timers still fire only once their deadline is reached.
 */
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
