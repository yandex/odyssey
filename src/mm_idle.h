#ifndef MM_IDLE_H_
#define MM_IDLE_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_idle_t mm_idle_t;

typedef void (*mm_idle_callback_t)(mm_idle_t*);

struct mm_idle_t {
	mm_idle_callback_t  callback;
	void               *arg;
};

#endif
