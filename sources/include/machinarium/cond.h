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

typedef struct mm_cond mm_cond_t;

enum {
	MM_COND_WAIT_FAIL = -1,
	MM_COND_WAIT_OK = 0,
	MM_COND_WAIT_OK_PROPAGATED = 1
};

struct mm_cond {
	mm_call_t call;
	mm_cond_t *propagate;
	/* is signal came directly or as for propagated cond? */
	int propagated;
};

static inline void mm_cond_init(mm_cond_t *cond)
{
	cond->propagate = NULL;
	cond->propagated = 0;
	memset(&cond->call, 0, sizeof(cond->call));
}

void mm_cond_signal(mm_cond_t *cond, mm_scheduler_t *sched);

static inline void mm_cond_propagate(mm_cond_t *src, mm_cond_t *dst)
{
	src->propagate = dst;
}

static inline int mm_cond_wait(mm_cond_t *cond, uint32_t time_ms)
{
	mm_errno_set(0);
	if (cond->call.type != MM_CALL_NONE) {
		mm_errno_set(EINPROGRESS);
		return MM_COND_WAIT_FAIL;
	}
	cond->propagated = 0;
	mm_call(&cond->call, MM_CALL_COND, time_ms);
	if (cond->call.status != 0) {
		return MM_COND_WAIT_FAIL;
	}

	if (cond->propagated) {
		cond->propagated = 0;
		return MM_COND_WAIT_OK_PROPAGATED;
	}

	return MM_COND_WAIT_OK;
}
