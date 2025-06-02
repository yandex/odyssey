#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

typedef struct mm_idle mm_idle_t;

typedef int (*mm_idle_callback_t)(mm_idle_t *);

struct mm_idle {
	mm_idle_callback_t callback;
	void *arg;
};

