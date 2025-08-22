/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium.h>
#include <machinarium_private.h>

static inline void sleepy_init(mm_sleepy_t *sleepy)
{
	if (mm_self == NULL || mm_self->scheduler.current == NULL) {
		abort();
	}

	sleepy->coro_id = mm_self->scheduler.current->id;
	sleepy->released = 0;
	atomic_store(&sleepy->refs, 0);

	mm_list_init(&sleepy->link);
	mm_eventmgr_add(&mm_self->event_mgr, &sleepy->event);
}

static inline void sleepy_ref(mm_sleepy_t *sleepy)
{
	atomic_fetch_add(&sleepy->refs, 1);
}

static inline void sleepy_unref(mm_sleepy_t *sleepy)
{
	if (atomic_fetch_sub(&sleepy->refs, 1) == 1) {
		mm_free(sleepy);
	}
}

static inline mm_sleepy_t *sleepy_create()
{
	mm_sleepy_t *sleepy = mm_malloc(sizeof(mm_sleepy_t));
	if (sleepy == NULL) {
		return NULL;
	}

	sleepy_init(sleepy);

	sleepy_ref(sleepy);

	return sleepy;
}

/* holding wait_list lock */
static inline void sleepy_add(mm_wait_list_t *wait_list, mm_sleepy_t *sleepy)
{
	mm_list_append(&wait_list->sleepies, &sleepy->link);
	++wait_list->sleepy_count;
}

/* holding wait_list lock */
static inline void sleepy_release(mm_wait_list_t *wait_list,
				  mm_sleepy_t *sleepy)
{
	if (!sleepy->released) {
		mm_list_unlink(&sleepy->link);
		--wait_list->sleepy_count;
		sleepy->released = 1;
	}
}

static inline void release_sleepy_with_lock(mm_wait_list_t *wait_list,
					    mm_sleepy_t *sleepy)
{
	mm_sleeplock_lock(&wait_list->lock);
	sleepy_release(wait_list, sleepy);
	mm_sleeplock_unlock(&wait_list->lock);
}

mm_wait_list_t *mm_wait_list_create(atomic_uint_fast64_t *word)
{
	mm_wait_list_t *wait_list = mm_malloc(sizeof(mm_wait_list_t));
	if (wait_list == NULL) {
		return NULL;
	}

	mm_sleeplock_init(&wait_list->lock);
	mm_list_init(&wait_list->sleepies);
	wait_list->sleepy_count = 0;
	wait_list->word = word;

	return wait_list;
}

void mm_wait_list_destroy(mm_wait_list_t *wait_list)
{
	mm_sleeplock_lock(&wait_list->lock);

	mm_list_t *i, *n;
	mm_list_foreach_safe(&wait_list->sleepies, i, n)
	{
		mm_sleepy_t *sleepy = mm_container_of(i, mm_sleepy_t, link);
		mm_call_cancel(&sleepy->event.call, NULL);

		sleepy_release(wait_list, sleepy);
	}

	mm_sleeplock_unlock(&wait_list->lock);

	mm_free(wait_list);
}

static inline int wait_sleepy(mm_wait_list_t *wait_list, mm_sleepy_t *sleepy,
			      uint32_t timeout_ms)
{
	mm_eventmgr_wait(&mm_self->event_mgr, &sleepy->event, timeout_ms);

	release_sleepy_with_lock(wait_list, sleepy);

	return sleepy->event.call.status;
}

int mm_wait_list_wait(mm_wait_list_t *wait_list, uint32_t timeout_ms)
{
	mm_sleepy_t *this = sleepy_create();
	if (this == NULL) {
		mm_errno_set(ENOMEM);
		return -1;
	}

	mm_sleeplock_lock(&wait_list->lock);
	sleepy_add(wait_list, this);
	mm_sleeplock_unlock(&wait_list->lock);

	int status = wait_sleepy(wait_list, this, timeout_ms);
	mm_errno_set(status);

	/* not in sleepies list here, so no sleepy_release */
	sleepy_unref(this);

	if (status != 0) {
		return -1;
	}
	return 0;
}

int mm_wait_list_compare_wait(mm_wait_list_t *wait_list, uint64_t expected,
			      uint32_t timeout_ms)
{
	mm_sleeplock_lock(&wait_list->lock);

	if (atomic_load(wait_list->word) != expected) {
		mm_sleeplock_unlock(&wait_list->lock);

		mm_errno_set(EAGAIN);
		return -1;
	}

	mm_sleepy_t *this = sleepy_create();
	if (this == NULL) {
		mm_errno_set(ENOMEM);
		return -1;
	}

	sleepy_add(wait_list, this);

	mm_sleeplock_unlock(&wait_list->lock);

	int status = wait_sleepy(wait_list, this, timeout_ms);
	mm_errno_set(status);

	/* not in sleepies list here, so no sleepy_release */
	sleepy_unref(this);

	if (status != 0) {
		return -1;
	}

	return 0;
}

void mm_wait_list_notify(mm_wait_list_t *wait_list)
{
	mm_sleeplock_lock(&wait_list->lock);

	if (wait_list->sleepy_count == 0ULL) {
		mm_sleeplock_unlock(&wait_list->lock);
		return;
	}

	mm_sleepy_t *sleepy;
	sleepy = mm_list_peek(wait_list->sleepies, mm_sleepy_t);

	sleepy_ref(sleepy);

	sleepy_release(wait_list, sleepy);

	mm_sleeplock_unlock(&wait_list->lock);

	int event_mgr_fd;
	event_mgr_fd = mm_eventmgr_signal(&sleepy->event);
	if (event_mgr_fd > 0) {
		mm_eventmgr_wakeup(event_mgr_fd);
	}

	sleepy_unref(sleepy);
}

void mm_wait_list_notify_all(mm_wait_list_t *wait_list)
{
	mm_sleepy_t *sleepy;

	mm_sleeplock_lock(&wait_list->lock);

	mm_list_t woken_sleepies;
	mm_list_init(&woken_sleepies);

	uint64_t count = wait_list->sleepy_count;
	for (uint64_t i = 0; i < count; ++i) {
		sleepy = mm_list_peek(wait_list->sleepies, mm_sleepy_t);

		sleepy_release(wait_list, sleepy);

		mm_list_append(&woken_sleepies, &sleepy->link);
	}

	mm_sleeplock_unlock(&wait_list->lock);

	for (uint64_t i = 0; i < count; ++i) {
		sleepy = mm_list_peek(woken_sleepies, mm_sleepy_t);
		mm_list_unlink(&sleepy->link);

		int event_mgr_fd;
		event_mgr_fd = mm_eventmgr_signal(&sleepy->event);
		if (event_mgr_fd > 0) {
			mm_eventmgr_wakeup(event_mgr_fd);
		}
	}
}

MACHINE_API machine_wait_list_t *
machine_wait_list_create(atomic_uint_fast64_t *word)
{
	mm_wait_list_t *wl;
	wl = mm_wait_list_create(word);
	if (wl == NULL) {
		return NULL;
	}

	return mm_cast(machine_wait_list_t *, wl);
}

MACHINE_API void machine_wait_list_destroy(machine_wait_list_t *wait_list)
{
	mm_wait_list_t *wl;
	wl = mm_cast(mm_wait_list_t *, wait_list);

	mm_wait_list_destroy(wl);
}

MACHINE_API int machine_wait_list_wait(machine_wait_list_t *wait_list,
				       uint32_t timeout_ms)
{
	mm_wait_list_t *wl;
	wl = mm_cast(mm_wait_list_t *, wait_list);

	int rc;
	rc = mm_wait_list_wait(wl, timeout_ms);

	return rc;
}

MACHINE_API int machine_wait_list_compare_wait(machine_wait_list_t *wait_list,
					       uint64_t value,
					       uint32_t timeout_ms)
{
	mm_wait_list_t *wl;
	wl = mm_cast(mm_wait_list_t *, wait_list);

	int rc;
	rc = mm_wait_list_compare_wait(wl, value, timeout_ms);

	return rc;
}

MACHINE_API void machine_wait_list_notify(machine_wait_list_t *wait_list)
{
	mm_wait_list_t *wl;
	wl = mm_cast(mm_wait_list_t *, wait_list);

	mm_wait_list_notify(wl);
}

MACHINE_API void machine_wait_list_notify_all(machine_wait_list_t *wait_list)
{
	mm_wait_list_t *wl;
	wl = mm_cast(mm_wait_list_t *, wait_list);

	mm_wait_list_notify_all(wl);
}
