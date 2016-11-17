#ifndef FT_FIBER_H_
#define FT_FIBER_H_

/*
 * flint.
 *
 * Cooperative multitasking engine.
*/

typedef struct ftfiberop ftfiberop;
typedef struct ftfiber ftfiber;

typedef enum {
	FT_FNEW,
	FT_FREADY,
	FT_FACTIVE,
	FT_FFREE
} ftfiberstate;

typedef void (*ftfiberf)(void *arg);
typedef void (*ftfibercancelf)(ftfiber*, void *arg);

struct ftfiberop {
	int             in_progress;
	ftfibercancelf  cancel;
	void           *arg;
};

struct ftfiber {
	uint64_t      id;
	ftfiberstate  state;
	ftfiberf      function;
	ftfiberop     op;
	int           cancel;
	void         *arg;
	ftcontext     context;
	ftfiber      *resume;
	uv_timer_t    timer;
	void         *scheduler;
	void         *data;
	ftlist        waiters;
	ftlist        link_wait;
	ftlist        link;
};

void ft_fiber_init(ftfiber*);

static inline int
ft_fiber_is_cancel(ftfiber *fiber) {
	return fiber->cancel;
}

static inline char*
ft_fiber_stackof(ftfiber *fiber) {
	return (char*)fiber + sizeof(ftfiber);
}

ftfiber*
ft_fiber_alloc(int);

void
ft_fiber_free(ftfiber*);

static inline void
ft_fiber_opbegin(ftfiber *fiber, ftfibercancelf cancel, void *arg)
{
	ftfiberop *op = &fiber->op;
	op->in_progress = 1;
	op->cancel = cancel;
	op->arg = arg;
}

static inline void
ft_fiber_opfinish(ftfiber *fiber)
{
	ftfiberop *op = &fiber->op;
	op->in_progress = 0;
	op->cancel = NULL;
	op->arg = NULL;
}

static inline void
ft_fiber_opcancel(ftfiber *fiber)
{
	ftfiberop *op = &fiber->op;
	if (fiber->cancel)
		return;
	fiber->cancel++;
	if (! op->in_progress)
		return;
	assert(op->cancel);
	op->cancel(fiber, op->arg);
}

#endif
