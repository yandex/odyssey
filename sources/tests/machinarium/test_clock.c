#include <machinarium/machinarium.h>
#include <machinarium/timer.h>
#include <machinarium/clock.h>
#include <tests/odyssey_test.h>

/*
 * The clock is driven with a synthetic time: time_cached is set so that
 * mm_clock_update() never overwrites time_ms with the real monotonic clock.
 */

typedef struct tm_t {
	mm_timer_t timer;
	int fired;
	uint64_t fired_at;
	/* optional: what to do with "other" from inside the callback */
	struct tm_t *other;
	int other_action; /* 0: nothing, 1: stop, 2: stop + restart */
	uint64_t other_interval;
} tm_t;

static inline uint64_t clock_now(mm_clock_t *clock)
{
	return clock->time_ms;
}

static inline void clock_set(mm_clock_t *clock, uint64_t ms)
{
	test(ms >= clock->time_ms);
	clock->time_ms = ms;
	clock->time_cached = 1;
}

static void clock_setup(mm_clock_t *clock, uint64_t start_ms)
{
	mm_clock_init(clock);
	clock->time_ms = start_ms;
	clock->time_cached = 1;
	clock->wheel_last_ms = start_ms;
}

static void on_fire(mm_timer_t *t)
{
	tm_t *x = mm_container_of(t, tm_t, timer);
	mm_clock_t *clock = (mm_clock_t *)t->clock;

	/* a timer must fire exactly once and never before its deadline */
	test(x->fired == 0);
	test(t->timeout <= clock_now(clock));
	test(t->active == 0);

	x->fired = 1;
	x->fired_at = clock_now(clock);

	if (x->other != NULL && x->other_action != 0) {
		tm_t *o = x->other;
		if (o->timer.active) {
			mm_timer_stop(&o->timer);
		}
		if (x->other_action == 2) {
			mm_timer_init(&o->timer, on_fire, NULL,
				      x->other_interval);
			o->fired = 0;
			mm_timer_start(clock, &o->timer);
		}
	}
}

static void tm_setup(tm_t *x, uint64_t interval)
{
	memset(x, 0, sizeof(*x));
	mm_timer_init(&x->timer, on_fire, NULL, interval);
}

static void tm_start(mm_clock_t *clock, tm_t *x, uint64_t interval)
{
	tm_setup(x, interval);
	mm_timer_start(clock, &x->timer);
}

/*
 * Advance the clock by following mm_clock_next_timeout(), like mm_loop_step
 * does, until the timer fires or limit is reached. Checks that the clock
 * never asks to sleep past the deadline of an armed timer.
 */
static void run_until_fired(mm_clock_t *clock, tm_t *x, uint64_t limit)
{
	while (!x->fired) {
		uint64_t next = mm_clock_next_timeout(clock);
		test(next != UINT64_MAX);
		test(next > clock_now(clock));
		test(next <= x->timer.timeout);
		test(next <= limit);
		clock_set(clock, next);
		mm_clock_step(clock);
	}
}

static void test_empty(void)
{
	mm_clock_t clock;
	clock_setup(&clock, 1000);

	test(clock.active == 0);
	test(clock.timers_count == 0);
	test(mm_clock_next_timeout(&clock) == UINT64_MAX);
	test(mm_clock_step(&clock) == 0);
	test(clock.active == 0);

	clock_set(&clock, 1000 + 123456789);
	test(mm_clock_step(&clock) == 0);
	test(mm_clock_next_timeout(&clock) == UINT64_MAX);

	mm_clock_free(&clock);
}

static void test_zero_interval(void)
{
	mm_clock_t clock;
	tm_t a;
	clock_setup(&clock, 777);

	tm_start(&clock, &a, 0);
	test(clock.active == 1);
	test(clock.timers_count == 1);
	test(a.timer.timeout == 777);

	/* already due: must fire without advancing the time */
	test(mm_clock_step(&clock) == 1);
	test(a.fired == 1);
	test(a.fired_at == 777);
	test(clock.timers_count == 0);
	test(clock.active == 0);

	/* zero interval added after a step at the same ms */
	tm_start(&clock, &a, 0);
	test(mm_clock_step(&clock) == 1);
	test(a.fired_at == 777);

	mm_clock_free(&clock);
}

static void test_single_exact(void)
{
	mm_clock_t clock;
	tm_t a;
	clock_setup(&clock, 100);

	tm_start(&clock, &a, 50);
	test(a.timer.timeout == 150);
	/* level 0: deadline is known exactly */
	test(mm_clock_next_timeout(&clock) == 150);

	test(mm_clock_step(&clock) == 0);
	clock_set(&clock, 149);
	test(mm_clock_step(&clock) == 0);
	test(a.fired == 0);

	clock_set(&clock, 150);
	test(mm_clock_step(&clock) == 1);
	test(a.fired == 1);
	test(a.fired_at == 150);
	test(clock.timers_count == 0);
	test(mm_clock_next_timeout(&clock) == UINT64_MAX);

	mm_clock_free(&clock);
}

/*
 * A timer which crosses a wheel window boundary is cascaded through
 * higher levels. Driven by next_timeout it must still fire exactly at
 * its deadline.
 */
static void test_cascade_levels(void)
{
	const uint64_t boundaries[] = {
		1ULL << 8, /* level 0 -> 1 */
		1ULL << 16, /* level 1 -> 2 */
		1ULL << 24, /* level 2 -> 3 */
	};

	for (size_t b = 0; b < sizeof(boundaries) / sizeof(boundaries[0]);
	     b++) {
		uint64_t base = 10 * boundaries[b];
		/* start a few ms before the boundary, deadline a few ms after */
		const uint64_t deltas[] = { 1,	  2,	 100,
					    255,  256,	 257,
					    1000, 70000, 0x1000000ULL + 5 };

		for (size_t d = 0; d < sizeof(deltas) / sizeof(deltas[0]);
		     d++) {
			mm_clock_t clock;
			tm_t a;
			uint64_t start = base - 3;

			clock_setup(&clock, start);
			tm_start(&clock, &a, deltas[d]);
			test(a.timer.timeout == start + deltas[d]);

			test(mm_clock_step(&clock) == 0);
			run_until_fired(&clock, &a, a.timer.timeout);
			test(a.fired_at == a.timer.timeout);

			mm_clock_free(&clock);
		}
	}
}

/*
 * Timers with different deadlines and a single big time jump across
 * several windows: all of them must fire in one step.
 */
static void test_time_jump(void)
{
	enum { N = 64 };
	mm_clock_t clock;
	tm_t t[N];
	uint64_t start = 123456;

	clock_setup(&clock, start);
	for (int i = 0; i < N; i++) {
		/* spread over level 0..3 */
		uint64_t interval = (uint64_t)1 << (i % 30);
		tm_start(&clock, &t[i], interval + i);
	}
	test(clock.timers_count == N);

	clock_set(&clock, start + (1ULL << 30));
	test(mm_clock_step(&clock) == N);
	for (int i = 0; i < N; i++) {
		test(t[i].fired == 1);
		test(t[i].timer.active == 0);
	}
	test(clock.timers_count == 0);
	test(clock.active == 0);

	mm_clock_free(&clock);
}

static void test_stop(void)
{
	mm_clock_t clock;
	tm_t a, b, c;
	clock_setup(&clock, 5000);

	tm_start(&clock, &a, 10);
	tm_start(&clock, &b, 10); /* same bucket as a */
	tm_start(&clock, &c, 100000); /* higher level */
	test(clock.timers_count == 3);

	mm_timer_stop(&a.timer);
	test(a.timer.active == 0);
	test(clock.timers_count == 2);
	test(clock.active == 1);
	/* stopping twice is a no-op */
	mm_timer_stop(&a.timer);
	test(clock.timers_count == 2);

	/* next timeout is still b's deadline, bucket mask must stay set */
	test(mm_clock_next_timeout(&clock) == 5010);

	clock_set(&clock, 5010);
	test(mm_clock_step(&clock) == 1);
	test(a.fired == 0);
	test(b.fired == 1);

	/* stop the only remaining timer: clock becomes inactive */
	mm_timer_stop(&c.timer);
	test(clock.timers_count == 0);
	test(clock.active == 0);
	test(mm_clock_next_timeout(&clock) == UINT64_MAX);

	/* stop the last timer in its bucket: mask must be cleared */
	tm_start(&clock, &a, 7);
	mm_timer_stop(&a.timer);
	tm_start(&clock, &b, 9);
	test(mm_clock_next_timeout(&clock) == 5019);

	/* restart a stopped timer */
	tm_start(&clock, &a, 3);
	test(mm_clock_next_timeout(&clock) == 5013);
	run_until_fired(&clock, &a, 5013);
	test(a.fired_at == 5013);
	run_until_fired(&clock, &b, 5019);
	test(b.fired_at == 5019);

	mm_clock_free(&clock);
}

/*
 * Timeouts of 2^32 ms and longer, and timers which cross a 2^32 ms
 * boundary of the highest wheel level, live in the overflow list.
 */
static void test_overflow(void)
{
	mm_clock_t clock;
	tm_t a, b, c;
	uint64_t start = (1ULL << 32) - 1000;

	clock_setup(&clock, start);

	tm_start(&clock, &a, 500); /* crosses 2^32 boundary at level 3 */
	tm_start(&clock, &b, (1ULL << 32) + 10); /* does not fit the wheel */
	tm_start(&clock, &c, 100); /* regular */
	test(clock.timers_count == 3);

	test(mm_clock_next_timeout(&clock) == c.timer.timeout);
	run_until_fired(&clock, &c, c.timer.timeout);
	test(c.fired_at == c.timer.timeout);

	test(a.fired == 0);
	run_until_fired(&clock, &a, a.timer.timeout);
	test(a.fired_at == a.timer.timeout);

	test(b.fired == 0);
	run_until_fired(&clock, &b, b.timer.timeout);
	test(b.fired_at == b.timer.timeout);

	test(clock.timers_count == 0);
	test(mm_list_empty(&clock.overflow_list));

	/* a timer with a deadline that would wrap uint64_t */
	tm_start(&clock, &a, UINT64_MAX);
	test(a.timer.timeout == UINT64_MAX);
	test(mm_clock_next_timeout(&clock) > clock_now(&clock));
	clock_set(&clock, clock_now(&clock) + (1ULL << 33));
	test(mm_clock_step(&clock) == 0);
	test(a.fired == 0);
	mm_timer_stop(&a.timer);
	test(clock.timers_count == 0);

	mm_clock_free(&clock);
}

/* A callback stops another timer which is due in the same step. */
static void test_stop_in_callback(void)
{
	mm_clock_t clock;
	tm_t a, b;
	clock_setup(&clock, 100);

	tm_start(&clock, &a, 10);
	tm_start(&clock, &b, 10);
	a.other = &b;
	a.other_action = 1;

	clock_set(&clock, 110);
	test(mm_clock_step(&clock) == 1);
	test(a.fired == 1);
	test(b.fired == 0);
	test(b.timer.active == 0);
	test(clock.timers_count == 0);
	test(clock.active == 0);

	mm_clock_free(&clock);
}

/*
 * A callback stops and re-arms another timer which is pending in the same
 * step. The re-armed timer must not fire now and must fire exactly at its
 * new deadline.
 */
static void test_restart_in_callback(void)
{
	mm_clock_t clock;
	tm_t a, b, c;
	clock_setup(&clock, 100);

	tm_start(&clock, &a, 10);
	tm_start(&clock, &b, 10);
	tm_start(&clock, &c, 10);
	a.other = &b;
	a.other_action = 2;
	a.other_interval = 500; /* crosses a level 0 window */

	clock_set(&clock, 110);
	test(mm_clock_step(&clock) == 2);
	test(a.fired == 1);
	test(c.fired == 1);
	test(b.fired == 0);
	test(b.timer.active == 1);
	test(b.timer.timeout == 610);
	test(clock.timers_count == 1);

	run_until_fired(&clock, &b, 610);
	test(b.fired_at == 610);
	test(clock.timers_count == 0);

	/* re-arm with zero interval: fires within the same step */
	clock_setup(&clock, 100);
	tm_start(&clock, &a, 10);
	tm_start(&clock, &b, 10);
	a.other = &b;
	a.other_action = 2;
	a.other_interval = 0;

	clock_set(&clock, 110);
	test(mm_clock_step(&clock) == 2);
	test(a.fired == 1);
	test(b.fired == 1);
	test(b.fired_at == 110);
	test(clock.timers_count == 0);

	mm_clock_free(&clock);
}

static void test_many_same_bucket(void)
{
	enum { N = 1000 };
	mm_clock_t clock;
	tm_t t[N];
	clock_setup(&clock, 42);

	for (int i = 0; i < N; i++) {
		tm_start(&clock, &t[i], 1000);
	}
	/* stop every other one */
	for (int i = 0; i < N; i += 2) {
		mm_timer_stop(&t[i].timer);
	}
	test(clock.timers_count == N / 2);

	run_until_fired(&clock, &t[1], 1042);
	for (int i = 0; i < N; i++) {
		test(t[i].fired == (i % 2));
		if (t[i].fired) {
			test(t[i].fired_at == 1042);
		}
	}
	test(clock.timers_count == 0);

	mm_clock_free(&clock);
}

/*
 * Randomized check against the trivial model: after every step no armed
 * timer may be overdue, next_timeout never exceeds the earliest deadline
 * and timers_count matches the number of active timers.
 */
static uint64_t random_interval(void)
{
	switch (rand() % 8) {
	case 0:
		return 0;
	case 1:
		return rand() % 4;
	case 2:
		return rand() % 300;
	case 3:
		return rand() % 70000;
	case 4:
		return rand() % 20000000;
	case 5:
		return 4294967295ULL - rand() % 1000; /* UINT32_MAX - x */
	case 6:
		return (1ULL << 32) + rand() % 1000; /* overflow */
	default:
		return rand() % 5000;
	}
}

static void test_random_one(uint64_t start, int iters, int restart_in_cb)
{
	enum { N = 512 };
	mm_clock_t clock;
	static tm_t t[N];
	uint64_t now = start;

	clock_setup(&clock, now);
	for (int i = 0; i < N; i++) {
		tm_setup(&t[i], 0);
	}

	for (int it = 0; it < iters; it++) {
		int ops = rand() % 16;
		for (int k = 0; k < ops; k++) {
			tm_t *x = &t[rand() % N];
			if (x->timer.active) {
				mm_timer_stop(&x->timer);
			} else {
				tm_start(&clock, x, random_interval());
				if (restart_in_cb && rand() % 4 == 0) {
					x->other = &t[rand() % N];
					x->other_action = 1 + rand() % 2;
					/*
					 * Non-zero: a chain of zero interval
					 * re-arms would never leave the step.
					 */
					x->other_interval =
						1 + random_interval();
				}
			}
		}

		uint64_t next = mm_clock_next_timeout(&clock);
		uint64_t adv;
		switch (rand() % 6) {
		case 0:
			adv = 0;
			break;
		case 1:
			adv = rand() % 1000000;
			break;
		case 2:
			adv = next == UINT64_MAX ? 1000 : next - now;
			break;
		default:
			adv = rand() % 300;
			break;
		}
		now += adv;
		clock_set(&clock, now);
		mm_clock_step(&clock);

		uint64_t count = 0;
		uint64_t min_timeout = UINT64_MAX;
		for (int i = 0; i < N; i++) {
			if (!t[i].timer.active) {
				continue;
			}
			count++;
			test(t[i].timer.timeout > now);
			if (t[i].timer.timeout < min_timeout) {
				min_timeout = t[i].timer.timeout;
			}
		}
		test(count == clock.timers_count);
		test(clock.active == (count != 0));

		next = mm_clock_next_timeout(&clock);
		test(next <= min_timeout);
		test(next == UINT64_MAX || next > now);
	}

	mm_clock_free(&clock);
}

static void test_random(void)
{
	srand(42);
	test_random_one(0, 20000, 0);
	test_random_one(1000, 20000, 0);
	test_random_one((1ULL << 32) - 500000, 20000, 0);
	test_random_one((1ULL << 32) - 500000, 20000, 1);
	test_random_one(1ULL << 40, 20000, 1);
}

void machinarium_test_clock(void)
{
	test_empty();
	test_zero_interval();
	test_single_exact();
	test_cascade_levels();
	test_time_jump();
	test_stop();
	test_overflow();
	test_stop_in_callback();
	test_restart_in_callback();
	test_many_same_bucket();
	test_random();
}
