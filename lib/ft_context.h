#ifndef FT_CONTEXT_H_
#define FT_CONTEXT_H_

/*
 * flint.
 *
 * Cooperative multitasking engine.
*/

typedef struct ftcontext ftcontext;

typedef void (*ftcontextf)(void*);

struct ftcontext {
	ucontext_t context;
};

void ft_context_init_main(ftcontext*);
void ft_context_init(ftcontext*, void*, int,
                     ftcontext*, ftcontextf, void*);
void ft_context_swap(ftcontext*, ftcontext*);

#endif
