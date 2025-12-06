#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium/channel.h>
#include <machinarium/event.h>
#include <machinarium/task.h>

typedef struct mm_taskmgr mm_taskmgr_t;

struct mm_taskmgr {
	int workers_count;
	int *workers;
	mm_channel_t channel;
	mm_event_t event;
};

void mm_taskmgr_init(mm_taskmgr_t *);
int mm_taskmgr_start(mm_taskmgr_t *, int);
void mm_taskmgr_stop(mm_taskmgr_t *);
int mm_taskmgr_new(mm_taskmgr_t *, mm_task_function_t, void *, uint32_t);
