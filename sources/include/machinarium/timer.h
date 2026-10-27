#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <stdint.h>
#include <stddef.h>

#include <machinarium/list.h>

typedef struct mm_timer mm_timer_t;

typedef void (*mm_timer_callback_t)(mm_timer_t *);

struct mm_timer {
	mm_list_t link;
	int active;
	int level;
	int slot;
	uint64_t timeout;
	uint64_t interval;
	uint32_t seq;
	mm_timer_callback_t callback;
	void *arg;
	void *clock;
};

static inline void mm_timer_init(mm_timer_t *timer, mm_timer_callback_t cb,
				 void *arg, uint64_t interval)
{
	timer->active = 0;
	timer->level = -1;
	timer->slot = 0;
	timer->interval = interval;
	timer->timeout = 0;
	timer->seq = 0;
	timer->callback = cb;
	timer->arg = arg;
	timer->clock = NULL;
	mm_list_init(&timer->link);
}
