
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

int mm_fiberstack_create(mm_fiberstack_t *stack, size_t size)
{
	stack->size = ((size * sizeof (void*) + 4096 - 1) / 4096) * 4096;
	stack->pointer = malloc(stack->size);
	if (stack->pointer == NULL)
		return -1;
	/*
	stack->valgrind_id =
		VALGRIND_STACK_REGISTER(stack->pointer, stack->pointer + stack->size);
	*/
	return 0;
}

void mm_fiberstack_free(mm_fiberstack_t *stack)
{
	if (stack->pointer == NULL)
		return;
	/*
	VALGRIND_STACK_DEREGISTER(stack->valgrind_id);
	*/
	free(stack->pointer);
}
