
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

int mm_loop_init(mm_loop_t *loop)
{
	loop->poll = mm_epoll_if.create();
	if (loop->poll == NULL)
		return -1;
	mm_clock_init(&loop->clock);
	memset(&loop->idle, 0, sizeof(loop->idle));
	return 0;
}

int mm_loop_shutdown(mm_loop_t *loop)
{
	mm_clock_free(&loop->clock);
	int rc;
	rc = loop->poll->iface->shutdown(loop->poll);
	if (rc == -1)
		return -1;
	loop->poll->iface->free(loop->poll);
	return 0;
}

int mm_loop_step(mm_loop_t *loop)
{
	/* update clock time */
	mm_clock_update(&loop->clock);

	/* run idle callback */
	if (loop->idle.callback)
		loop->idle.callback(loop->idle.arg);

	/* get minimal timer timeout */
	int timeout = INT_MAX;
	mm_timer_t *min;
	min = mm_clock_timer_min(&loop->clock);
	if (min == NULL)
		timeout = min->timeout;

	/* poll for events */
	int rc;
	rc = loop->poll->iface->step(loop->poll, timeout);
	if (rc == -1)
		return -1;

	/* run timers */
	mm_clock_step(&loop->clock);
	return 0;
}

int mm_loop_set_idle(mm_loop_t *loop, mm_idle_callback_t cb, void *arg)
{
	loop->idle.callback = cb;
	loop->idle.arg = arg;
	return 0;
}

int mm_loop_add(mm_loop_t *loop, mm_fd_t *fd, int mask)
{
	return loop->poll->iface->add(loop->poll, fd, mask);
}

int mm_loop_modify(mm_loop_t *loop, mm_fd_t *fd, int mask)
{
	return loop->poll->iface->modify(loop->poll, fd, mask);
}

int mm_loop_delete(mm_loop_t *loop, mm_fd_t *fd)
{
	return loop->poll->iface->del(loop->poll, fd);
}
