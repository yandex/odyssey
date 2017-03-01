#ifndef MM_SCHEDULER_H_
#define MM_SCHEDULER_H_

/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

typedef struct mmscheduler mmscheduler;

struct mmscheduler {
	mmlist    list_ready;
	mmlist    list_active;
	mmlist    list_free;
	int       count_ready;
	int       count_active;
	int       count_free;
	int       size_stack;
	mmfiber   main;
	mmfiber  *current;
	uint64_t  seq;
	void     *data;
};

static inline mmfiber*
mm_scheduler_current(mmscheduler *s) {
	return s->current;
}

static inline mmfiber*
mm_scheduler_next_ready(mmscheduler *s)
{
	if (s->count_ready == 0)
		return NULL;
	return mm_container_of(s->list_ready.next, mmfiber, link);
}

static inline int
mm_scheduler_online(mmscheduler *s) {
	return s->count_active + s->count_ready;
}

int  mm_scheduler_init(mmscheduler*, int, void*);
void mm_scheduler_free(mmscheduler*);

mmfiber*
mm_scheduler_new(mmscheduler*, mmfiberf, void*);

mmfiber*
mm_scheduler_match(mmscheduler*, uint64_t);

void mm_scheduler_set(mmfiber*, mmfiberstate);
void mm_scheduler_call(mmfiber*);
void mm_scheduler_yield(mmscheduler*);
void mm_scheduler_wait(mmfiber*, mmfiber*);

#endif
