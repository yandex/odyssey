#ifndef MM_FIBER_STACK_H_
#define MM_FIBER_STACK_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_fiberstack_t mm_fiberstack_t;

struct mm_fiberstack_t {
	char   *pointer;
	size_t  size;
};

int  mm_fiberstack_create(mm_fiberstack_t*, size_t);
void mm_fiberstack_free(mm_fiberstack_t*);

#endif
