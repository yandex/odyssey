#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium/machine_mgr.h>
#include <machinarium/task_mgr.h>
#include <machinarium/thread_pool.h>

typedef struct mm_config mm_config_t;
typedef struct mm mm_t;

struct mm_config {
	int page_size;
	int stack_size;
	int system_stack_size;
	int pool_size;
	int coroutine_cache_size;
	int msg_cache_gc_size;
	int msg_cache_gc_count;
	uint64_t dns_ttl_ms;
	int file_pool_size;
};

struct mm {
	mm_config_t config;
	mm_machinemgr_t machine_mgr;
	mm_taskmgr_t task_mgr;
	/*
	 * The global file I/O pool. Workers are machines created by
	 * mm_thread_pool_init(), so the pool must be started after the
	 * machine manager is ready and stopped before it is freed.
	 * NULL when not running (fallback to synchronous file calls).
	 */
	mm_thread_pool_t *file_pool;
};

extern mm_t machinarium;
