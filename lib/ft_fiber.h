#ifndef FT_FIBER_H_
#define FT_FIBER_H_

/*
 * flint.
 *
 * Cooperative multitasking engine.
*/

typedef struct ftfiber ftfiber;

typedef enum {
	FT_FNEW,
	FT_FREADY,
	FT_FACTIVE,
	FT_FFREE
} ftfiberstate;

typedef void (*ftfiberf)(void *arg);

struct ftfiber {
	uint64_t      id;
	ftfiberstate  state;
	ftfiberf      function;
	void         *arg;
	ftcontext     context;
	ftfiber      *resume;
	uv_timer_t    timer;
	void         *scheduler;
	void         *data;
	ftlist        waiters;
	ftlist        link_wait;
	ftlist        link;
};

void ft_fiber_init(ftfiber*);

static inline char*
ft_fiber_stackof(ftfiber *fiber) {
	return (char*)fiber + sizeof(ftfiber);
}

ftfiber*
ft_fiber_alloc(int);

void
ft_fiber_free(ftfiber*);

#endif
