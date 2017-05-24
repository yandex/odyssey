#ifndef MM_H_
#define MM_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_t mm_t;

struct mm_t {
	mm_machinemgr_t machine_mgr;
	mm_msgcache_t   msg_cache;
	mm_taskmgr_t    task_mgr;
};

extern mm_t machinarium;

#endif
