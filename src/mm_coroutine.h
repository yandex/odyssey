#ifndef MM_COROUTINE_H_
#define MM_COROUTINE_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_coroutine_t mm_coroutine_t;

typedef void (*mm_function_t)(void *arg);

typedef enum {
	MM_COROUTINE_NEW,
	MM_COROUTINE_READY,
	MM_COROUTINE_ACTIVE,
	MM_COROUTINE_FREE
} mm_coroutinestate_t;

struct mm_coroutine_t {
	uint64_t            id;
	mm_coroutinestate_t state;
	int                 cancel;
	mm_function_t       function;
	void               *function_arg;
	mm_contextstack_t   stack;
	mm_context_t        context;
	mm_coroutine_t     *resume;
	void               *call_ptr;
	mm_list_t           joiners;
	mm_list_t           link_join;
	mm_list_t           link;
};

mm_coroutine_t*
mm_coroutine_allocate(int);

void mm_coroutine_init(mm_coroutine_t*);
void mm_coroutine_free(mm_coroutine_t*);
void mm_coroutine_cancel(mm_coroutine_t*);

static inline int
mm_coroutine_is_cancelled(mm_coroutine_t *coroutine) {
	return coroutine->cancel;
}

#endif
