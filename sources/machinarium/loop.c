
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <string.h>

#include <machinarium/machinarium.h>
#include <machinarium/loop.h>

/*
 * Do not sleep longer than one second even if the next timer is far away.
 * This keeps the value safely within epoll_wait()/kevent() int timeout and
 * bounds the loop reaction to a clock jump. One extra wakeup per second is
 * negligible for the Odyssey event loop.
 */
#define MM_LOOP_MAX_SLEEP_MS 1000

#if defined(__linux__)
#include <machinarium/epoll.h>
#define MM_POLL_IF mm_epoll_if
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
	defined(__OpenBSD__) || defined(__DragonFly__)
#include <machinarium/kqueue.h>
#define MM_POLL_IF mm_kqueue_if
#else
#error "machinarium: unsupported platform, no poll backend available"
#endif

int mm_loop_init(mm_loop_t *loop)
{
	loop->poll = MM_POLL_IF.create();
	if (loop->poll == NULL) {
		return -1;
	}
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
	if (rc == -1) {
		return -1;
	}
	loop->poll->iface->free(loop->poll);
	return 0;
}

int mm_loop_step(mm_loop_t *loop)
{
	/* update clock time */
	mm_clock_reset(&loop->clock);
	if (loop->clock.active) {
		mm_clock_update(&loop->clock);
	}

	/* run idle callback */
	int rc;
	if (loop->idle.callback) {
		rc = loop->idle.callback(&loop->idle);
		if (!rc) {
			return 0;
		}
	}

	/* run timers */
	int fired = mm_clock_step(&loop->clock);

	/*
	 * Get timeout until the next timer. If some timers have just fired,
	 * poll with zero timeout so the woken coroutines are scheduled
	 * immediately. In case of no timers - just poll for 1 sec,
	 * this will not create cpu load.
	 */
	int timeout_ms = 0;
	if (fired == 0) {
		timeout_ms = MM_LOOP_MAX_SLEEP_MS;

		uint64_t next = mm_clock_next_timeout(&loop->clock);
		if (next != UINT64_MAX) {
			uint64_t now = loop->clock.time_ms;
			uint64_t diff = next > now ? next - now : 0;

			if (diff < MM_LOOP_MAX_SLEEP_MS) {
				timeout_ms = (int)diff;
			}
		}
	}

	/* poll for events */
	rc = loop->poll->iface->step(loop->poll, timeout_ms);
	if (rc == -1) {
		return -1;
	}

	return 0;
}
