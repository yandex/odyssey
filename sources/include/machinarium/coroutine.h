#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <stdint.h>

#include <machinarium/context_stack.h>
#include <machinarium/context.h>
#include <machinarium/list.h>

#define MM_COROUTINE_MAX_NAME_LEN 15

typedef struct mm_coroutine mm_coroutine_t;

typedef void (*mm_function_t)(void *arg);

typedef enum { MM_CNEW, MM_CREADY, MM_CACTIVE, MM_CFREE } mm_coroutinestate_t;

struct mm_coroutine {
	uint64_t id;
	mm_coroutinestate_t state;
	int cancel;
	int errno_;
	mm_function_t function;
	void *function_arg;
	mm_contextstack_t stack;
	mm_context_t context;
	mm_coroutine_t *resume;
	void *call_ptr;
	mm_list_t joiners;
	mm_list_t link_join;
	mm_list_t link;
	char name[MM_COROUTINE_MAX_NAME_LEN + 1];
};

mm_coroutine_t *mm_coroutine_allocate(int, int);

void mm_coroutine_init(mm_coroutine_t *);
void mm_coroutine_free(mm_coroutine_t *);
void mm_coroutine_cancel(mm_coroutine_t *);
void mm_coroutine_set_name(mm_coroutine_t *, const char *);
const char *mm_coroutine_get_name(mm_coroutine_t *);

static inline int mm_coroutine_is_cancelled(mm_coroutine_t *coroutine)
{
	return coroutine->cancel;
}
