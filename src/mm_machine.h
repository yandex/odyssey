#ifndef MM_MACHINE_H
#define MM_MACHINE_H

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_machine mm_machine_t;

struct mm_machine
{
	int                 online;
	uint64_t            id;
	char               *name;
	machine_function_t  main;
	void               *main_arg;
	mm_thread_t         thread;
	mm_scheduler_t      scheduler;
	mm_queuerdcache_t   queuerd_cache;
	mm_loop_t           loop;
	mm_list_t           link;
};

extern __thread mm_machine_t *mm_self;

#endif /* MM_MACHINE_H */
