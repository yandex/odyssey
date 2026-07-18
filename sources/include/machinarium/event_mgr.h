#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium/fd.h>
#include <machinarium/sleep_lock.h>
#include <machinarium/list.h>
#include <machinarium/loop.h>
#include <machinarium/event.h>

typedef struct mm_eventmgr_t mm_eventmgr_t;

struct mm_eventmgr_t {
	mm_fd_t fd;
#if !defined(__linux__)
	int wakeup_fd;
#endif
	mm_sleeplock_t lock;
	mm_list_t list_ready;
	mm_list_t list_wait;
	int count_ready;
	int count_wait;
};

int mm_eventmgr_init(mm_eventmgr_t *, mm_loop_t *);
void mm_eventmgr_free(mm_eventmgr_t *, mm_loop_t *);
void mm_eventmgr_add(mm_eventmgr_t *, mm_event_t *);
int mm_eventmgr_wait(mm_eventmgr_t *, mm_event_t *, uint32_t);
int mm_eventmgr_signal(mm_event_t *);
void mm_eventmgr_wakeup(int);

static inline int mm_eventmgr_wakeup_fd(mm_eventmgr_t *mgr)
{
#if defined(__linux__)
	return mgr->fd.fd;
#else
	return mgr->wakeup_fd;
#endif
}
