#ifndef MM_CLOCK_H
#define MM_CLOCK_H

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_clock mm_clock_t;

struct mm_clock
{
	uint64_t time;
	mm_buf_t timers;
	int      timers_count;
	int      timers_seq;
};

void mm_clock_init(mm_clock_t*);
void mm_clock_free(mm_clock_t*);
void mm_clock_update(mm_clock_t*);
int  mm_clock_step(mm_clock_t*);
int  mm_clock_timer_add(mm_clock_t*, mm_timer_t*);
int  mm_clock_timer_del(mm_clock_t*, mm_timer_t*);

mm_timer_t*
mm_clock_timer_min(mm_clock_t*);

static inline void
mm_timer_start(mm_clock_t *clock,
               mm_timer_t *timer,
               mm_timer_callback_t cb, void *arg, int interval)
{
	mm_timer_init(timer, cb, arg, interval);
	mm_clock_timer_add(clock, timer);
}

static inline void
mm_timer_stop(mm_timer_t *timer)
{
	if (! timer->active)
		return;
	mm_clock_timer_del(timer->clock, timer);
}

#endif /* MM_CLOCK_H */
