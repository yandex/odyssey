#ifndef MM_TIMER_H_
#define MM_TIMER_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_timer_t mm_timer_t;

typedef int (*mm_timer_callback_t)(mm_timer_t*);

struct mm_timer_t {
	int                  timeout;
	int                  seq;
	mm_timer_callback_t  callback;
	void                *arg;
};

#endif
