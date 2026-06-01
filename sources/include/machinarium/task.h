#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium/event.h>

typedef struct mm_task mm_task_t;

typedef struct mm_taskmgr_worker {
	int64_t id;
	mm_list_t dns_cache;
	uint64_t dns_ttl_ms;
} mm_taskmgr_worker_t;

typedef void (*mm_task_function_t)(mm_taskmgr_worker_t *w, void *);

struct mm_task {
	mm_task_function_t function;
	void *arg;
	mm_event_t on_complete;
};
