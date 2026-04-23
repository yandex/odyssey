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
	/* is signal came directly or as for propagated cond? */
	int propagated;

#ifndef NDEBUG
	/*
	 * must always be used from one machine
	 */
	mm_scheduler_t *owner;
#endif
} mm_cond_awaiter_t;

struct mm_cond {
	mm_list_t awaiters;
	mm_cond_t *propagate;

#ifndef NDEBUG
	/*
	 * must always be used from one machine
	 */
	mm_scheduler_t *owner;
#endif
};

static inline void mm_cond_init(mm_cond_t *cond)
{
	cond->propagate = NULL;
	mm_list_init(&cond->awaiters);

#ifndef NDEBUG
	cond->owner = NULL;
#endif
}

void mm_cond_signal(mm_cond_t *cond);

static inline void mm_cond_propagate(mm_cond_t *src, mm_cond_t *dst)
{
	src->propagate = dst;
}

/*
 * spirious wakeups are possible
 * (when awaiting one cond from several coroutines)
 */
int mm_cond_wait(mm_cond_t *cond, uint32_t time_ms);
