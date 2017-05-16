#ifndef MM_CONTEXT_H_
#define MM_CONTEXT_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef void (*mm_context_callback_t)(void*);

void *mm_context_alloc(void);
void  mm_context_free(void*);
void  mm_context_create(void*, void*,
                        mm_fiberstack_t*,
                        mm_context_callback_t, void*);
void  mm_context_swap(void*, void*);

#endif
