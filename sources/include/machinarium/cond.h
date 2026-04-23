#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <machinarium/call.h>
#include <machinarium/scheduler.h>
#include <machinarium/machine.h>
#include <machinarium/list.h>

typedef struct mm_cond mm_cond_t;

enum {
	MM_COND_WAIT_FAIL = -1,
	MM_COND_WAIT_OK = 0,
	MM_COND_WAIT_OK_PROPAGATED = 1
};

typedef struct {
	mm_list_t link;
	mm_call_t call;
} mm_cond_awaiter_t;

struct mm_cond {
	mm_list_t awaiters;
	mm_cond_t *propagate;
	/* is signal came directly or as for propagated cond? */
	int propagated;
};

static inline void mm_cond_init(mm_cond_t *cond)
{
	cond->propagate = NULL;
	cond->propagated = 0;
	mm_list_init(&cond->awaiters);
}

void mm_cond_signal(mm_cond_t *cond, mm_scheduler_t *sched);

static inline void mm_cond_propagate(mm_cond_t *src, mm_cond_t *dst)
{
	src->propagate = dst;
}

/*
 * spirious wakeups are possible
 * (when awaiting one cond from several coroutines)
 */
int mm_cond_wait(mm_cond_t *cond, uint32_t time_ms);
