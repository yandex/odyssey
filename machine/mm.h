#ifndef MM_H_
#define MM_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_t mm_t;

struct mm_t {
	int          online;
	/*mmscheduler  scheduler;*/
	uv_loop_t    loop;
	uv_prepare_t prepare;
};

/*
static inline mmfiber*
mm_current(mm *f) {
	return f->scheduler.current;
}

static inline void
mm_wakeup(mm *f, mmfiber *fiber)
{
	mm_scheduler_set(fiber, MM_FREADY);
	(void)f;
}

static inline void
mm_wakeup_waiters(mm *f, mmfiber *fiber)
{
	mmfiber *waiter;
	mmlist *i;
	mm_listforeach(&fiber->waiters, i) {
		waiter = mm_container_of(i, mmfiber, link_wait);
		mm_scheduler_set(waiter, MM_FREADY);
	}
	(void)f;
}
*/

#endif
