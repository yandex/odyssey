
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <string.h>

#include <machinarium/build.h>
#include <machinarium/machinarium.h>
#include <machinarium/context.h>

#ifdef USE_UCONTEXT
#include <ucontext.h>
#endif

#ifdef HAVE_TSAN
#include <sanitizer/tsan_interface.h>
#endif

typedef struct {
	mm_context_t *context_runner;
	mm_context_t *context;
	mm_context_function_t function;
	void *arg;
} mm_runner_t;

static __thread mm_runner_t runner;

#ifdef HAVE_TSAN
void mm_context_swap(mm_context_t *current, mm_context_t *new)
{
	current->tsan_fiber = __tsan_get_current_fiber();
	if (current->destroying) {
		new->exit_from = current;
	}
	__tsan_switch_to_fiber(new->tsan_fiber, 0);

	mm_context_swap_private(current, new);

	if (current->exit_from != NULL) {
		__tsan_destroy_fiber(current->exit_from->tsan_fiber);
		current->exit_from = NULL;
	}
}
#endif

static void mm_context_runner(void)
{
	/* save argument */
	volatile mm_runner_t runner_arg = runner;

	/* return to mm_context_create() */
	mm_context_swap(runner_arg.context, runner_arg.context_runner);

	runner_arg.function(runner_arg.arg);
	abort();
}

#ifdef USE_UCONTEXT

static inline void mm_context_prepare_ucontext(ucontext_t *uctx,
					       mm_contextstack_t *stack)
{
	/* actually, there are no errors defined in docs */
	getcontext(uctx);

	uctx->uc_link = NULL;
	uctx->uc_stack.ss_sp = stack->pointer;
	uctx->uc_stack.ss_size = stack->size;

	makecontext(uctx, mm_context_runner, 0);
}

#else

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

#endif /* USE_UCONTEXT */

void mm_context_create(mm_context_t *context, mm_contextstack_t *stack,
		       void (*function)(void *), void *arg)
{
	/* must be first */
	mm_context_t context_runner;

#ifdef HAVE_TSAN
	context_runner.exit_from = NULL;
	context_runner.destroying = 0;
#endif

	/* prepare context runner */
	runner.context_runner = &context_runner;
	runner.context = context;
	runner.function = function;
	runner.arg = arg;

	/* prepare context */
#ifdef USE_UCONTEXT
	mm_context_prepare_ucontext(&context->uctx, stack);
#else
	context->sp = mm_context_prepare(stack);
#endif /* USE_UCONTEXT */

#ifdef HAVE_TSAN
	context->tsan_fiber = __tsan_create_fiber(0);
	context->exit_from = NULL;
	context->destroying = 0;
#endif

	/* execute runner: pass function and argument */
	mm_context_swap(&context_runner, context);
}
