#ifndef MM_SCHEDULER_H_
#define MM_SCHEDULER_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_scheduler_t mm_scheduler_t;

struct mm_scheduler_t {
	mm_fiber_t *current;
	mm_fiber_t  main;
	int         count_ready;
	int         count_active;
	int         count_free;
	mm_list_t   list_ready;
	mm_list_t   list_active;
	mm_list_t   list_free;
	int         size_stack;
	void       *data;
};

static inline mm_fiber_t*
mm_scheduler_current(mm_scheduler_t *scheduler) {
	return scheduler->current;
}

static inline int
mm_scheduler_online(mm_scheduler_t *scheduler) {
	return scheduler->count_active + scheduler->count_ready;
}

int  mm_scheduler_init(mm_scheduler_t*, int, void*);
void mm_scheduler_free(mm_scheduler_t*);

mm_fiber_t*
mm_scheduler_new(mm_scheduler_t*, mm_function_t, void*);

void mm_scheduler_set(mm_fiber_t*, mm_fiberstate_t);
void mm_scheduler_call(mm_fiber_t*);
void mm_scheduler_yield(mm_scheduler_t*);
void mm_scheduler_wait(mm_fiber_t*, mm_fiber_t*);

static inline mm_fiber_t*
mm_scheduler_next_ready(mm_scheduler_t *scheduler)
{
	if (scheduler->count_ready == 0)
		return NULL;
	return mm_container_of(scheduler->list_ready.next, mm_fiber_t, link);
}

static inline void
mm_scheduler_wakeup(mm_fiber_t *fiber)
{
	mm_scheduler_set(fiber, MM_FIBER_READY);
}

static inline void
mm_scheduler_wakeup_waiters(mm_fiber_t *fiber)
{
	mm_fiber_t *waiter;
	mm_list_t *i;
	mm_list_foreach(&fiber->waiters, i) {
		waiter = mm_container_of(i, mm_fiber_t, link_wait);
		mm_scheduler_set(waiter, MM_FIBER_READY);
	}
}

#endif
