#ifndef MM_CONTEXT_H_
#define MM_CONTEXT_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

void *mm_context_alloc(size_t);
void  mm_context_free(void*);
void  mm_context_create(void*, void (*)(void*), void*);
void  mm_context_swap(void*, void*);

#endif
