#ifndef MM_FIBER_H_
#define MM_FIBER_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_fiber_t mm_fiber_t;

typedef void (*mm_function_t)(void *arg);

typedef enum {
	MM_FIBER_NEW,
	MM_FIBER_READY,
	MM_FIBER_ACTIVE,
	MM_FIBER_FREE
} mm_fiberstate_t;

struct mm_fiber_t {
	uint64_t           id;
	mm_fiberstate_t    state;
	int                cancel;
	mm_function_t      function;
	void              *function_arg;
	mm_contextstack_t  stack;
	mm_context_t       context;
	mm_fiber_t        *resume;
	void              *call_ptr;
	mm_list_t          joiners;
	mm_list_t          link_channel;
	mm_list_t          link_join;
	mm_list_t          link;
};

mm_fiber_t*
mm_fiber_allocate(int);

void mm_fiber_init(mm_fiber_t*);
void mm_fiber_free(mm_fiber_t*);
void mm_fiber_cancel(mm_fiber_t*);

static inline int
mm_fiber_is_cancelled(mm_fiber_t *fiber) {
	return fiber->cancel;
}

#endif
