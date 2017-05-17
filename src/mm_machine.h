#ifndef MM_MACHINE_H_
#define MM_MACHINE_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_machine_t mm_machine_t;

struct mm_machine_t {
	int            online;
	mm_scheduler_t scheduler;
	mm_loop_t      loop;
};

#endif
