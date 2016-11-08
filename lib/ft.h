#ifndef FT_H_
#define FT_H_

/*
 * fluent.
 *
 * Cooperative multitasking engine.
*/

typedef struct ft ft;

struct ft {
	int         online;
	ftscheduler scheduler;
	uv_loop_t   loop;
	uv_async_t  async;
};

static inline ftfiber*
ft_current(ft *f) {
	return f->scheduler.current;
}

static inline void
ft_wakeup(ft *f, ftfiber *fiber) {
	ft_scheduler_set(fiber, FT_FREADY);
	uv_async_send(&f->async);
}

#endif
