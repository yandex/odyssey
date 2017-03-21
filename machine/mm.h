#ifndef MM_H_
#define MM_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_t mm_t;

struct mm_t {
	int            online;
	mm_scheduler_t scheduler;
	uv_loop_t      loop;
	uv_prepare_t   prepare;
};

#endif
