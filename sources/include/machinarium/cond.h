#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

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
	int propagated;
} mm_cond_awaiter_t;

struct mm_cond {
	mm_list_t awaiters;
	mm_cond_t *propagate;
#ifndef NDEBUG
	/*
	 * Cond must always be used from one machine (one cooperative
	 * scheduler). Set on first mm_cond_wait, cleared when the awaiters
	 * list empties so the cond can migrate to another machine after
	 * od_io_detach / od_io_attach.
	 */
	mm_machine_t *owner;
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

void mm_cond_signal(mm_cond_t *cond, mm_scheduler_t *sched);

static inline void mm_cond_propagate(mm_cond_t *src, mm_cond_t *dst)
{
	src->propagate = dst;
}

/*
 * Spurious wakeups are possible when awaiting one cond from several
 * coroutines.
 */
int mm_cond_wait(mm_cond_t *cond, uint32_t time_ms);
