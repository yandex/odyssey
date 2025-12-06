
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <string.h>

#include <machinarium/machinarium.h>
#include <machinarium/context.h>

typedef struct {
	mm_context_t *context_runner;
	mm_context_t *context;
	mm_context_function_t function;
	void *arg;
} mm_runner_t;

static __thread mm_runner_t runner;

static void mm_context_runner(void)
{
	/* save argument */
	volatile mm_runner_t runner_arg = runner;

	/* return to mm_context_create() */
	mm_context_swap(runner_arg.context, runner_arg.context_runner);

	runner_arg.function(runner_arg.arg);
	abort();
}

static inline void write_func_ptr(void **dst, void (*func)(void))
{
	memcpy(dst, &func, sizeof(func));
}

static inline void **mm_context_prepare(mm_contextstack_t *stack)
{
	void **sp;
	sp = (void **)(stack->pointer + stack->size);
#if __amd64
	*--sp = NULL;
	/* eq to *--sp = (void *)mm_context_runner */
	write_func_ptr(--sp, mm_context_runner);
	/* for x86_64 we need to place return address on stack */
	sp -= 6;
	memset(sp, 0, sizeof(void *) * 6);
#elif __aarch64__
	/* for aarch64 we need to place return address in x30 reg */
	sp -= 16;
	memset(sp, 0, sizeof(void *) * 16);
	/* eq to *(sp + 1) = (void *)mm_context_runner; */
	write_func_ptr(sp + 1, mm_context_runner);
#else
	*--sp = NULL;
	/* eq to *--sp = (void *)mm_context_runner; */
	write_func_ptr(--sp, mm_context_runner);
	sp -= 4;
	memset(sp, 0, sizeof(void *) * 4);
#endif
	return sp;
}

void mm_context_create(mm_context_t *context, mm_contextstack_t *stack,
		       void (*function)(void *), void *arg)
{
	/* must be first */
	mm_context_t context_runner;

	/* prepare context runner */
	runner.context_runner = &context_runner;
	runner.context = context;
	runner.function = function;
	runner.arg = arg;

	/* prepare context */
	context->sp = mm_context_prepare(stack);

	/* execute runner: pass function and argument */
	mm_context_swap(&context_runner, context);
}
