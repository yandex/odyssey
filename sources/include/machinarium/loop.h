#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium/clock.h>
#include <machinarium/idle.h>
#include <machinarium/poll.h>

typedef struct mm_loop mm_loop_t;

struct mm_loop {
	mm_clock_t clock;
	mm_idle_t idle;
	mm_poll_t *poll;
};

int mm_loop_init(mm_loop_t *);
int mm_loop_shutdown(mm_loop_t *);
int mm_loop_step(mm_loop_t *);

static inline void mm_loop_set_idle(mm_loop_t *loop, mm_idle_callback_t cb,
				    void *arg)
{
	loop->idle.callback = cb;
	loop->idle.arg = arg;
}

static inline int mm_loop_add(mm_loop_t *loop, mm_fd_t *fd,
			      mm_fd_callback_t on_read, void *on_read_arg,
			      mm_fd_callback_t on_write, void *on_write_arg,
			      mm_fd_callback_t on_err, void *on_err_arg,
			      mm_fd_callback_t on_close, void *on_close_arg)
{
	return loop->poll->iface->add(loop->poll, fd, on_read, on_read_arg,
				      on_write, on_write_arg, on_err,
				      on_err_arg, on_close, on_close_arg);
}

static inline int mm_loop_add_ro(mm_loop_t *loop, mm_fd_t *fd,
				 mm_fd_callback_t on_read, void *on_read_arg)
{
	return mm_loop_add(loop, fd, on_read, on_read_arg, NULL, NULL, NULL,
			   NULL, NULL, NULL);
}

static inline int mm_loop_delete(mm_loop_t *loop, mm_fd_t *fd)
{
	return loop->poll->iface->del(loop->poll, fd);
}
