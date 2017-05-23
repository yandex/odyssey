#ifndef MM_TASK_MGR_H_
#define MM_TASK_MGR_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_taskmgr_t mm_taskmgr_t;

struct mm_taskmgr_t {
	int        workers_count;
	int       *workers;
	mm_queue_t queue;
};

void mm_taskmgr_init(mm_taskmgr_t*);
int  mm_taskmgr_start(mm_taskmgr_t*, int);
void mm_taskmgr_stop(mm_taskmgr_t*);
int  mm_taskmgr_new(mm_taskmgr_t*, mm_task_function_t, void*, int);

#endif
