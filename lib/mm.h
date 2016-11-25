#ifndef MM_H_
#define MM_H_

/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

typedef struct mm mm;

struct mm {
	int         online;
	mmscheduler scheduler;
	uv_loop_t   loop;
	uv_async_t  async;
};

static inline mmfiber*
mm_current(mm *f) {
	return f->scheduler.current;
}

static inline void
mm_wakeup(mm *f, mmfiber *fiber)
{
	mm_scheduler_set(fiber, MM_FREADY);
	uv_async_send(&f->async);
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
	uv_async_send(&f->async);
}

#endif
