#ifndef MM_TASK_H_
#define MM_TASK_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_task_t mm_task_t;

typedef void (*mm_task_function_t)(void*);

struct mm_task_t {
	mm_task_function_t function;
	void              *arg;
	mm_queue_t         on_complete;
};

#endif
