#ifndef MM_OPERATION_H_
#define MM_OPERATION_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_operation_t mm_operation_t;

typedef void (*mm_cancel_t)(void*, void *arg);

struct mm_operation_t {
	int         active;
	mm_cancel_t cancel_function;
	void       *arg;
};

static inline void
mm_operation_init(mm_operation_t *op)
{
	op->active = 0;
	op->cancel_function = NULL;
	op->arg = NULL;
}

static inline void
mm_operation_begin(mm_operation_t *op, mm_cancel_t function, void *arg)
{
	op->active = 1;
	op->cancel_function = function;
	op->arg = arg;
}

static inline void
mm_operation_end(mm_operation_t *op)
{
	op->active = 0;
	op->cancel_function = NULL;
	op->arg = NULL;
}

static inline void
mm_operation_cancel(mm_operation_t *op, void *arg)
{
	if (! op->active)
		return;
	op->cancel_function(arg, op->arg);
}

#endif
