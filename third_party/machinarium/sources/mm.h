#ifndef MM_H
#define MM_H

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

typedef struct mm_config mm_config_t;
typedef struct mm mm_t;

struct mm_config {
	int page_size;
	int stack_size;
	int pool_size;
	int coroutine_cache_size;
	int msg_cache_gc_size;
};

struct mm {
	mm_config_t config;
	mm_machinemgr_t machine_mgr;
	mm_taskmgr_t task_mgr;

	uint64_t coro_id_seq;
};

extern mm_t machinarium;

uint64_t mm_next_coro_id();

#endif /* MM_H */
