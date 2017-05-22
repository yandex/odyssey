#ifndef MM_MACHINE_H_
#define MM_MACHINE_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_machine_t mm_machine_t;

struct mm_machine_t {
	int                 online;
	int                 id;
	char               *name;
	machine_function_t  main;
	void               *main_arg;
	mm_thread_t         thread;
	mm_scheduler_t      scheduler;
	mm_queuerdpool_t    queuerd_pool;
	mm_loop_t           loop;
	mm_list_t           link;
};

extern __thread mm_machine_t *mm_self;

#endif
