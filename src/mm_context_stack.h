#ifndef MM_CONTEXT_STACK_H_
#define MM_CONTEXT_STACK_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_contextstack_t mm_contextstack_t;

struct mm_contextstack_t {
	char   *pointer;
	size_t  size;
#ifdef HAVE_VALGRIND
	int     valgrind_stack;
#endif
};

int  mm_contextstack_create(mm_contextstack_t*, size_t);
void mm_contextstack_free(mm_contextstack_t*);

#endif
