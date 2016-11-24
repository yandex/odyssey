#ifndef MM_CONTEXT_H_
#define MM_CONTEXT_H_

/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

typedef struct mmcontext mmcontext;

typedef void (*mmcontextf)(void*);

struct mmcontext {
	ucontext_t context;
};

void mm_context_init_main(mmcontext*);
void mm_context_init(mmcontext*, void*, int,
                     mmcontext*, mmcontextf, void*);
void mm_context_swap(mmcontext*, mmcontext*);

#endif
