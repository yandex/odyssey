#ifndef MM_TIMER_H_
#define MM_TIMER_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_timer_t  mm_timer_t;
typedef struct mm_timers_t mm_timers_t;

typedef int (*mm_timer_callback_t)(mm_timer_t*);

struct mm_timer_t {
	int                  time;
	mm_timer_callback_t  callback;
	void                *arg;
};

struct mm_timers_t {
	mm_timer_t **list;
	int count;
};

void mm_timers_init(mm_timers_t*);
void mm_timers_free(mm_timers_t*);
int  mm_timers_add(mm_timers_t*, mm_timer_t*);
int  mm_timers_del(mm_timers_t*, mm_timer_t*);

#endif
