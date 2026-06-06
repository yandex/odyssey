/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>

#include <machinarium/machinarium.h>
#include <machinarium/wait_list.h>
#include <machinarium/machine.h>
#include <stdatomic.h>

/* holding wait_list lock */
static inline void add_sleepy(mm_wait_list_t *wait_list, mm_sleepy_t *sleepy)
{
	mm_list_append(&wait_list->sleepies, &sleepy->link);
	++wait_list->sleepy_count;
}

/* holding wait_list lock */
static inline void release_sleepy(mm_wait_list_t *wait_list,
				  mm_sleepy_t *sleepy)
{
	if (!sleepy->released) {
		mm_list_unlink(&sleepy->link);
		--wait_list->sleepy_count;
		sleepy->released = 1;
	}
}

static inline void init_sleepy(mm_sleepy_t *sleepy, void *private)
{
	if (mm_self == NULL || mm_self->scheduler.current == NULL) {
		abort();
	}

	sleepy->coro_id = mm_self->scheduler.current->id;
	sleepy->private = private;

	sleepy->released = 0;

	mm_list_init(&sleepy->link);
	mm_eventmgr_add(&mm_self->event_mgr, &sleepy->event);
}

static inline void release_sleepy_with_lock(mm_wait_list_t *wait_list,
					    mm_sleepy_t *sleepy)
{
	mm_sleeplock_lock(&wait_list->lock);
	release_sleepy(wait_list, sleepy);
	mm_sleeplock_unlock(&wait_list->lock);
}

void mm_wait_list_init(mm_wait_list_t *wait_list, atomic_uint_fast64_t *word)
{
	mm_sleeplock_init(&wait_list->lock);
	mm_list_init(&wait_list->sleepies);
	wait_list->sleepy_count = 0;
	wait_list->word = word;
}

mm_wait_list_t *mm_wait_list_create(atomic_uint_fast64_t *word)
{
	mm_wait_list_t *wait_list = mm_malloc(sizeof(mm_wait_list_t));
	if (wait_list == NULL) {
		return NULL;
	}

	mm_wait_list_init(wait_list, word);

	return wait_list;
}

void mm_wait_list_free(mm_wait_list_t *wait_list)
{
	mm_wait_list_destroy(wait_list);
	mm_free(wait_list);
}

void mm_wait_list_destroy(mm_wait_list_t *wait_list)
{
	(void)wait_list;
}

static inline int wait_sleepy(mm_wait_list_t *wait_list, mm_sleepy_t *sleepy,
			      uint32_t timeout_ms)
{
	mm_eventmgr_wait(&mm_self->event_mgr, &sleepy->event, timeout_ms);

	release_sleepy_with_lock(wait_list, sleepy);

	return sleepy->event.call.status;
}

int mm_wait_list_wait(mm_wait_list_t *wait_list, void *private,
		      uint32_t timeout_ms)
{
	mm_sleepy_t this;
	init_sleepy(&this, private);

	mm_sleeplock_lock(&wait_list->lock);
	add_sleepy(wait_list, &this);
	mm_sleeplock_unlock(&wait_list->lock);

	int status = wait_sleepy(wait_list, &this, timeout_ms);
	mm_errno_set(status);
	if (status != 0) {
		return -1;
	}
	return 0;
}

int mm_wait_list_compare_wait(mm_wait_list_t *wait_list, void *private,
			      uint64_t expected, uint32_t timeout_ms)
{
	mm_sleeplock_lock(&wait_list->lock);

	if (atomic_load(wait_list->word) != expected) {
		mm_sleeplock_unlock(&wait_list->lock);

		mm_errno_set(EAGAIN);
		return -1;
	}

	mm_sleepy_t this;
	init_sleepy(&this, private);

	add_sleepy(wait_list, &this);

	mm_sleeplock_unlock(&wait_list->lock);

	int status = wait_sleepy(wait_list, &this, timeout_ms);
	mm_errno_set(status);
	if (status != 0) {
		return -1;
	}
	return 0;
}


void *mm_wait_list_notify_cb(mm_wait_list_t *wait_list, mm_wl_private_cb_t cb,
			     void *arg)
{
	mm_sleeplock_lock(&wait_list->lock);

	mm_sleepy_t *sleepy = NULL;
	int event_mgr_fd = 0;
	void *private = NULL;

	if (wait_list->sleepy_count > 0ULL) {
		sleepy = mm_list_peek(wait_list->sleepies, mm_sleepy_t);

		release_sleepy(wait_list, sleepy);

		event_mgr_fd = mm_eventmgr_signal(&sleepy->event);

		private = sleepy->private;

		if (cb != NULL) {
			cb(private, arg);
		}
	} else {
		/* no sleepy to wake, still call cb if provided */
		(void)cb;
		(void)arg;
	}

	/* notify all registered listeners (listeners are invoked under wait_list lock) */
	mm_list_t *j;
	mm_list_foreach (&wait_list->listeners, j) {
		mm_wait_list_listener_t *listener;
		listener = mm_container_of(j, mm_wait_list_listener_t, link);
		if (listener->cb) {
			listener->cb(listener->arg);
		}
	}

	mm_sleeplock_unlock(&wait_list->lock);

	if (event_mgr_fd > 0) {
		mm_eventmgr_wakeup(event_mgr_fd);
	}

	return private;
}

void *mm_wait_list_notify(mm_wait_list_t *wait_list)
{
	return mm_wait_list_notify_cb(wait_list, NULL, NULL);
}

int mm_wait_list_notify_all(mm_wait_list_t *wait_list)
{
	int signaled = 0;

	mm_sleeplock_lock(&wait_list->lock);

	uint64_t count = wait_list->sleepy_count;

	int *event_mgr_fds = mm_malloc(sizeof(int) * count);
	if (event_mgr_fds == NULL) {
		abort();
	}

	if (count > 0) {
		signaled = 1;
	}

	mm_sleepy_t *sleepy;
	for (uint64_t i = 0; i < count; ++i) {
		sleepy = mm_list_peek(wait_list->sleepies, mm_sleepy_t);

		release_sleepy(wait_list, sleepy);

		event_mgr_fds[i] = mm_eventmgr_signal(&sleepy->event);
	}

	mm_list_t *j;
	mm_list_foreach (&wait_list->listeners, j) {
		mm_wait_list_listener_t *listener;
		listener = mm_container_of(j, mm_wait_list_listener_t, link);
		if (listener->cb) {
			listener->cb(listener->arg);
			signaled = 1;
		}
	}


	mm_sleeplock_unlock(&wait_list->lock);

	for (uint64_t i = 0; i < count; ++i) {
		if (event_mgr_fds[i] > 0) {
			mm_eventmgr_wakeup(event_mgr_fds[i]);
		}
	}

	mm_free(event_mgr_fds);

	return signaled;
}





void mm_wait_list_add_listener(mm_wait_list_t *wait_list, mm_wait_list_listener_t *listener)
{
	mm_sleeplock_lock(&wait_list->lock);
	mm_list_init(&listener->link);
	mm_list_append(&wait_list->listeners, &listener->link);
	mm_sleeplock_unlock(&wait_list->lock);
}

void mm_wait_list_remove_listener(mm_wait_list_t *wait_list, mm_wait_list_listener_t *listener)
{
	mm_sleeplock_lock(&wait_list->lock);
	/* unlink if linked */
	mm_list_unlink(&listener->link);
	mm_sleeplock_unlock(&wait_list->lock);
}

struct mm_waitv_cb_arg {
	size_t idx;
	size_t *which;
	mm_event_t *event;
	int *fired;
};

static void mm_waitv_listener_cb(void *arg)
{
	struct mm_waitv_cb_arg *p = arg;
	/* try to mark fired only once */
	if (__sync_bool_compare_and_swap(p->fired, 0, 1)) {
		if (p->which) {
			*p->which = p->idx;
		}
		int fd = mm_eventmgr_signal(p->event);
		if (fd > 0) {
			mm_eventmgr_wakeup(fd);
		}
	}
}

int mm_wait_list_waitv(mm_wait_list_t **wait_lists, void **privates, size_t count,
					   uint32_t timeout_ms, size_t *index_out)
{
	(void)privates; /* reserved for future use */

	if (count == 0 || wait_lists == NULL) {
		mm_errno_set(EINVAL);
		return -1;
	}

	/* prepare shared event for this waiter */
	mm_event_t shared_event;
	mm_eventmgr_add(&mm_self->event_mgr, &shared_event);

	mm_wait_list_listener_t *listeners = mm_malloc(sizeof(*listeners) * count);
	if (listeners == NULL) {
		mm_errno_set(ENOMEM);
		return -1;
	}
	struct mm_waitv_cb_arg *args = mm_malloc(sizeof(*args) * count);
	if (args == NULL) {
		mm_free(listeners);
		mm_errno_set(ENOMEM);
		return -1;
	}
	int *fired = mm_malloc(sizeof(int));
	if (fired == NULL) {
		mm_free(listeners);
		mm_free(args);
		mm_errno_set(ENOMEM);
		return -1;
	}
	*fired = 0;

	/* register listeners on all provided wait_lists */
	for (size_t i = 0; i < count; ++i) {
		mm_wait_list_listener_t *l = &listeners[i];
		mm_list_init(&l->link);
		l->cb = mm_waitv_listener_cb;
		args[i].idx = i;
		args[i].which = index_out;
		args[i].event = &shared_event;
		args[i].fired = fired;
		l->arg = &args[i];
		mm_wait_list_add_listener(wait_lists[i], l);
	}

	/* wait for any listener to signal shared_event */
	(void)mm_eventmgr_wait(&mm_self->event_mgr, &shared_event, timeout_ms);

	/* remove listeners (safe even if they already fired) */
	for (size_t i = 0; i < count; ++i) {
		mm_wait_list_remove_listener(wait_lists[i], &listeners[i]);
	}

	int result;
	if (*fired) {
		/* some listener fired and set index_out */
		result = 0;
	} else {
		/* timeout or nothing fired */
		mm_errno_set(ETIMEDOUT);
		result = -1;
	}

	mm_free(listeners);
	mm_free(args);
	mm_free(fired);

	return result;
}


