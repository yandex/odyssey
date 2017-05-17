
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
	mm_clock_update(&loop->clock);
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
		loop->idle.callback(&loop->idle);

	/* get minimal timer timeout */
	int timeout = INT_MAX;
	mm_timer_t *min;
	min = mm_clock_timer_min(&loop->clock);
	if (min)
		timeout = min->interval;

	/* run timers */
	mm_clock_step(&loop->clock);

	/* poll for events */
	int rc;
	rc = loop->poll->iface->step(loop->poll, timeout);
	if (rc == -1)
		return -1;

	/* update clock time */
	mm_clock_update(&loop->clock);

	/* run timers */
	mm_clock_step(&loop->clock);
	return 0;
}
