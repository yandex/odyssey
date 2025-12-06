#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <stdatomic.h>
#include <stdint.h>

#include <machinarium/machinarium.h>
#include <machinarium/coroutine.h>
#include <machinarium/scheduler.h>
#include <machinarium/thread.h>
#include <machinarium/signal_mgr.h>
#include <machinarium/event_mgr.h>
#include <machinarium/coroutine_cache.h>
#include <machinarium/msg_cache.h>
#include <machinarium/loop.h>
#include <machinarium/list.h>

typedef struct mm_machine mm_machine_t;

struct mm_machine {
	atomic_int online;
	uint64_t id;
	char *name;
	machine_coroutine_t main;
	void *main_arg;
	mm_thread_t thread;
	void *thread_global_private;
	mm_scheduler_t scheduler;
	mm_signalmgr_t signal_mgr;
	mm_eventmgr_t event_mgr;
	mm_msgcache_t msg_cache;
	mm_coroutine_cache_t coroutine_cache;
	mm_loop_t loop;
	mm_list_t link;
	struct mm_tls_ctx *server_tls_ctx;
	struct mm_tls_ctx *client_tls_ctx;
};

extern __thread mm_machine_t *mm_self;

static inline void mm_errno_set(int value)
{
	mm_scheduler_current(&mm_self->scheduler)->errno_ = value;
}

static inline int mm_errno_get(void)
{
	return mm_scheduler_current(&mm_self->scheduler)->errno_;
}
