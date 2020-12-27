
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium.h>
#include <machinarium_private.h>

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

static inline void **mm_context_prepare(mm_contextstack_t *stack)
{
	void **sp;
	sp = (void **)(stack->pointer + stack->size);
	*--sp = NULL;
	*--sp = (void *)mm_context_runner;
#if __amd64
	sp -= 6;
	memset(sp, 0, sizeof(void *) * 6);
#else
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

#if !defined(__amd64) && !defined(__i386)
#error unsupported architecture
#endif

asm("\t.text\n"
    "\t.globl mm_context_swap\n"
    "\t.type x,@function\n"
    "mm_context_swap:\n"
#if __amd64
    "\tpushq %rbp\n"
    "\tpushq %rbx\n"
    "\tpushq %r12\n"
    "\tpushq %r13\n"
    "\tpushq %r14\n"
    "\tpushq %r15\n"
    "\tmovq %rsp, (%rdi)\n"
    "\tmovq (%rsi), %rsp\n"
    "\tpopq %r15\n"
    "\tpopq %r14\n"
    "\tpopq %r13\n"
    "\tpopq %r12\n"
    "\tpopq %rbx\n"
    "\tpopq %rbp\n"
    "\tret\n"
#elif __i386
    "\tpushl %ebp\n"
    "\tpushl %ebx\n"
    "\tpushl %esi\n"
    "\tpushl %edi\n"
    "\tmovl %esp, (%eax)\n"
    "\tmovl (%edx), %esp\n"
    "\tpopl %edi\n"
    "\tpopl %esi\n"
    "\tpopl %ebx\n"
    "\tpopl %ebp\n"
    "\tret\n"
#endif
);
