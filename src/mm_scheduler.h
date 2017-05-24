#ifndef MM_SCHEDULER_H_
#define MM_SCHEDULER_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_scheduler_t mm_scheduler_t;

struct mm_scheduler_t {
	mm_coroutine_t *current;
	mm_coroutine_t  main;
	int             count_ready;
	int             count_active;
	int             count_free;
	mm_list_t       list_ready;
	mm_list_t       list_active;
	mm_list_t       list_free;
	int             size_stack;
	uint64_t        id_seq;
};

static inline mm_coroutine_t*
mm_scheduler_current(mm_scheduler_t *scheduler) {
	return scheduler->current;
}

static inline int
mm_scheduler_online(mm_scheduler_t *scheduler) {
	return scheduler->count_active + scheduler->count_ready;
}

int  mm_scheduler_init(mm_scheduler_t*, int);
void mm_scheduler_free(mm_scheduler_t*);
void mm_scheduler_run(mm_scheduler_t*);

mm_coroutine_t*
mm_scheduler_new(mm_scheduler_t*, mm_function_t, void*);

mm_coroutine_t*
mm_scheduler_find(mm_scheduler_t*, uint64_t);

void mm_scheduler_set(mm_scheduler_t*, mm_coroutine_t*, mm_coroutinestate_t);
void mm_scheduler_call(mm_scheduler_t*, mm_coroutine_t*);
void mm_scheduler_yield(mm_scheduler_t*);
void mm_scheduler_join(mm_coroutine_t*, mm_coroutine_t*);

static inline void
mm_scheduler_wakeup(mm_scheduler_t *scheduler, mm_coroutine_t *coroutine)
{
	mm_scheduler_set(scheduler, coroutine, MM_COROUTINE_READY);
}

#endif
