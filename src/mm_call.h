#ifndef MM_CALL_H_
#define MM_CALL_H_

/*
 * machinarium.
 *
 * cocallerative multitasking engine.
*/

typedef struct mm_call_t mm_call_t;

typedef void (*mm_cancel_t)(void*, void *arg);

struct mm_call_t {
	int          active;
	mm_cancel_t  cancel_function;
	void        *arg;
};

static inline void
mm_call_init(mm_call_t *call)
{
	call->active = 0;
	call->cancel_function = NULL;
	call->arg = NULL;
}

static inline void
mm_call_begin(mm_call_t *call, mm_cancel_t function, void *arg)
{
	call->active = 1;
	call->cancel_function = function;
	call->arg = arg;
}

static inline void
mm_call_end(mm_call_t *call)
{
	call->active = 0;
	call->cancel_function = NULL;
	call->arg = NULL;
}

static inline void
mm_call_cancel(mm_call_t *call, void *arg)
{
	if (! call->active)
		return;
	call->cancel_function(arg, call->arg);
}

#endif
