#ifndef MM_H
#define MM_H

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm mm_t;

struct mm
{
	mm_machinemgr_t      machine_mgr;
	mm_msgcache_t        msg_cache;
	mm_coroutine_cache_t coroutine_cache;
	mm_taskmgr_t         task_mgr;
};

extern mm_t machinarium;

#endif /* MM_H */
