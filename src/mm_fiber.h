#ifndef MM_FIBER_H_
#define MM_FIBER_H_

/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

typedef struct mmfiberop mmfiberop;
typedef struct mmfiber mmfiber;

typedef enum {
	MM_FNEW,
	MM_FREADY,
	MM_FACTIVE,
	MM_FFREE
} mmfiberstate;

typedef void (*mmfiberf)(void *arg);
typedef void (*mmfibercancelf)(mmfiber*, void *arg);

struct mmfiberop {
	int             in_progress;
	mmfibercancelf  cancel;
	void           *arg;
};

struct mmfiber {
	uint64_t      id;
	mmfiberstate  state;
	mmfiberf      function;
	mmfiberop     op;
	int           cancel;
	void         *arg;
	mmcontext     context;
	mmfiber      *resume;
	uv_timer_t    timer;
	int           condition;
	int           condition_status;
	void         *scheduler;
	void         *data;
	mmlist        waiters;
	mmlist        link_wait;
	mmlist        link;
};

void mm_fiber_init(mmfiber*);

static inline int
mm_fiber_is_cancel(mmfiber *fiber) {
	return fiber->cancel;
}

static inline char*
mm_fiber_stackof(mmfiber *fiber) {
	return (char*)fiber + sizeof(mmfiber);
}

mmfiber*
mm_fiber_alloc(int);

void
mm_fiber_free(mmfiber*);

static inline void
mm_fiber_timer_stop(mmfiber *fiber)
{
	if (! uv_is_closing((uv_handle_t*)&fiber->timer))
		uv_close((uv_handle_t*)&fiber->timer, NULL);
}

static inline void
mm_fiber_op_begin(mmfiber *fiber, mmfibercancelf cancel, void *arg)
{
	mmfiberop *op = &fiber->op;
	op->in_progress = 1;
	op->cancel = cancel;
	op->arg = arg;
}

static inline void
mm_fiber_op_end(mmfiber *fiber)
{
	mmfiberop *op = &fiber->op;
	op->in_progress = 0;
	op->cancel = NULL;
	op->arg = NULL;
}

static inline void
mm_fiber_cancel(mmfiber *fiber)
{
	mmfiberop *op = &fiber->op;
	if (fiber->cancel)
		return;
	fiber->cancel++;
	if (! op->in_progress)
		return;
	assert(op->cancel);
	op->cancel(fiber, op->arg);
}

#endif
