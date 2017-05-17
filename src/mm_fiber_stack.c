
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

#ifdef HAVE_VALGRIND
#  include <valgrind/valgrind.h>
#endif

int mm_fiberstack_create(mm_fiberstack_t *stack, size_t size)
{
	stack->size = ((size * sizeof (void*) + 4096 - 1) / 4096) * 4096;
	stack->pointer = malloc(stack->size);
	if (stack->pointer == NULL)
		return -1;
#ifdef HAVE_VALGRIND
	stack->valgrind_stack =
		VALGRIND_STACK_REGISTER(stack->pointer, stack->pointer + stack->size);
#endif
	return 0;
}

void mm_fiberstack_free(mm_fiberstack_t *stack)
{
	if (stack->pointer == NULL)
		return;
#ifdef HAVE_VALGRIND
	VALGRIND_STACK_DEREGISTER(stack->valgrind_stack);
#endif
	free(stack->pointer);
}
