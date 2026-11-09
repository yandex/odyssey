/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>
#include <string.h>

#include <machinarium/machinarium.h>
#include <machinarium/wait_list.h>
#include <machinarium/machine.h>
#include <machinarium/port.h>
#include <stdatomic.h>

/* holding wait_list lock */
static inline mm_list_t *wl_shard_head(mm_wait_list_t *wait_list, int shard)
{
	if (wait_list->shards == NULL) {
		return &wait_list->base.head;
	}
	return &wait_list->shards[shard].head;
}

/* holding wait_list lock */
static inline void add_sleepy(mm_wait_list_t *wait_list, mm_sleepy_t *sleepy)
{
	sleepy->shard = (int)(mm_self->id % (uint64_t)wait_list->n_shards);
	sleepy->seq = ++wait_list->seq;
	mm_list_t *head = wl_shard_head(wait_list, sleepy->shard);
	if (wait_list->nonempty != NULL && mm_list_empty(head)) {
		wait_list->nonempty[sleepy->shard >> 6] |=
			1ULL << (sleepy->shard & 63);
	}
	mm_list_append(head, &sleepy->link);
	atomic_fetch_add(&wait_list->sleepy_count, 1);
}

/* holding wait_list lock */
static inline mm_sleepy_t *wl_peek_oldest(mm_wait_list_t *wait_list)
{
	if (wait_list->shards == NULL) {
		if (mm_list_empty(&wait_list->base.head)) {
			return NULL;
		}
		return mm_container_of(wait_list->base.head.next, mm_sleepy_t,
				       link);
	}

	mm_sleepy_t *oldest = NULL;
	int words = (wait_list->n_shards + 63) / 64;
	int w;
	for (w = 0; w < words; w++) {
		uint64_t bits = wait_list->nonempty[w];
		while (bits) {
			int shard = w * 64 + mm_ctz64(bits);
			bits &= bits - 1;

			mm_list_t *head = &wait_list->shards[shard].head;
			mm_sleepy_t *sleepy =
				mm_container_of(head->next, mm_sleepy_t, link);
			if (oldest == NULL || sleepy->seq < oldest->seq) {
				oldest = sleepy;
			}
		}
	}
	return oldest;
}

/* holding wait_list lock */
static inline void release_sleepy(mm_wait_list_t *wait_list,
				  mm_sleepy_t *sleepy)
{
	if (!sleepy->released) {
		mm_list_unlink(&sleepy->link);
		atomic_fetch_sub(&wait_list->sleepy_count, 1);
		sleepy->released = 1;

		if (wait_list->nonempty != NULL) {
			mm_list_t *head =
				wl_shard_head(wait_list, sleepy->shard);
			if (mm_list_empty(head)) {
				wait_list->nonempty[sleepy->shard >> 6] &=
					~(1ULL << (sleepy->shard & 63));
			}
		}
	}
}

static inline void init_sleepy(mm_sleepy_t *sleepy, void *private,
			       mm_event_t *event)
{
	if (mm_self == NULL || mm_self->scheduler.current == NULL) {
		abort();
	}

	sleepy->coro_id = mm_self->scheduler.current->id;
	sleepy->private = private;

	sleepy->released = 0;

	mm_list_init(&sleepy->link);
	if (event) {
		sleepy->type = SHARED;
		sleepy->shared_event = event;
	} else {
		sleepy->type = EXCLUSIVE;
		mm_eventmgr_add(&mm_self->event_mgr, &sleepy->event);
	}
}

static inline void release_sleepy_with_lock(mm_wait_list_t *wait_list,
					    mm_sleepy_t *sleepy)
{
	mm_spinlock_lock(&wait_list->lock);
	release_sleepy(wait_list, sleepy);
	mm_spinlock_unlock(&wait_list->lock);
}

void mm_wait_list_init(mm_wait_list_t *wait_list, atomic_uint_fast64_t *word)
{
	mm_spinlock_init(&wait_list->lock);
	mm_list_init(&wait_list->base.head);
	wait_list->shards = NULL;
	wait_list->n_shards = 1;
	wait_list->seq = 0;
	wait_list->sleepy_count = 0;
	wait_list->nonempty = NULL;
	wait_list->word = word;
}

void mm_wait_list_init_sharded(mm_wait_list_t *wait_list,
			       atomic_uint_fast64_t *word, uint64_t *nonempty,
			       uint64_t *served, mm_wl_shard_t *shards,
			       int n_shards)
{
	mm_wait_list_init(wait_list, word);
	wait_list->nonempty = nonempty;
	wait_list->served = served;
	wait_list->shards = shards;
	wait_list->n_shards = n_shards;
	int i;
	for (i = 0; i < n_shards; i++) {
		mm_list_init(&shards[i].head);
	}
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
	init_sleepy(&this, private, NULL);

	mm_spinlock_lock(&wait_list->lock);
	add_sleepy(wait_list, &this);
	mm_spinlock_unlock(&wait_list->lock);

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
	mm_spinlock_lock(&wait_list->lock);

	if (atomic_load(wait_list->word) != expected) {
		mm_spinlock_unlock(&wait_list->lock);

		mm_errno_set(EAGAIN);
		return -1;
	}

	mm_sleepy_t this;
	init_sleepy(&this, private, NULL);

	add_sleepy(wait_list, &this);

	/*
	 * Re-check the word after enqueueing (futex-style protocol):
	 * a notify() which checks sleepy_count without holding the lock
	 * may miss us only if the word has changed already, and in that
	 * case we must not sleep.
	 */
	if (atomic_load(wait_list->word) != expected) {
		release_sleepy(wait_list, &this);

		mm_spinlock_unlock(&wait_list->lock);

		mm_eventmgr_remove(&mm_self->event_mgr, &this.event);

		mm_errno_set(EAGAIN);
		return -1;
	}

	mm_spinlock_unlock(&wait_list->lock);

	int status = wait_sleepy(wait_list, &this, timeout_ms);
	mm_errno_set(status);
	if (status != 0) {
		return -1;
	}
	return 0;
}

int wakeup_event_from_sleepy(mm_sleepy_t *sleepy)
{
	if (sleepy->type == EXCLUSIVE) {
		return mm_eventmgr_signal(&sleepy->event);
	} else {
		return mm_eventmgr_signal(sleepy->shared_event);
	}
}

void *mm_wait_list_notify_cb(mm_wait_list_t *wait_list, mm_wl_private_cb_t cb,
			     void *arg)
{
	mm_spinlock_lock(&wait_list->lock);

	if (atomic_load(&wait_list->sleepy_count) == 0ULL) {
		mm_spinlock_unlock(&wait_list->lock);
		return NULL;
	}

	mm_sleepy_t *sleepy;
	sleepy = wl_peek_oldest(wait_list);
	release_sleepy(wait_list, sleepy);

	int event_mgr_fd;
	event_mgr_fd = wakeup_event_from_sleepy(sleepy);

	void *private = sleepy->private;

	if (cb != NULL) {
		cb(private, arg);
	}

	mm_spinlock_unlock(&wait_list->lock);

	if (event_mgr_fd > 0) {
		mm_eventmgr_wakeup(event_mgr_fd);
	}

	return private;
}

/* holding wait_list lock; returns UINT64_MAX when no foreign shard has
 * a waiter */
static inline uint64_t wl_min_foreign_served(mm_wait_list_t *wait_list,
					     int my_shard)
{
	uint64_t min_served = UINT64_MAX;
	int words = (wait_list->n_shards + 63) / 64;
	int w;
	for (w = 0; w < words; w++) {
		uint64_t bits = wait_list->nonempty[w];
		while (bits) {
			int shard = w * 64 + mm_ctz64(bits);
			bits &= bits - 1;

			if (shard == my_shard) {
				continue;
			}
			if (wait_list->served[shard] < min_served) {
				min_served = wait_list->served[shard];
			}
		}
	}
	return min_served;
}

void *mm_wait_list_notify_cb_affinity(mm_wait_list_t *wait_list,
				      mm_wl_private_cb_t cb, void *arg)
{
	mm_sleepy_t *sleepy = NULL;

	mm_spinlock_lock(&wait_list->lock);

	if (atomic_load(&wait_list->sleepy_count) != 0ULL) {
		int my_shard =
			(int)(mm_self->id % (uint64_t)wait_list->n_shards);
		mm_list_t *head = wl_shard_head(wait_list, my_shard);
		if (mm_list_empty(head)) {
			sleepy = wl_peek_oldest(wait_list);
		} else {
			sleepy = mm_container_of(head->next, mm_sleepy_t, link);

			/*
			 * served-lag guard: when our shard has been served
			 * MM_WL_SERVICE_LAG times more often than the most
			 * lagging non-empty foreign shard, serve the
			 * globally oldest waiter instead, so no shard can
			 * starve of wakeups and slots can migrate between
			 * the worker chains
			 */
			if (wait_list->served != NULL) {
				uint64_t min_foreign = wl_min_foreign_served(
					wait_list, my_shard);
				if (min_foreign != UINT64_MAX &&
				    wait_list->served[my_shard] >
					    min_foreign + MM_WL_SERVICE_LAG) {
					mm_sleepy_t *oldest =
						wl_peek_oldest(wait_list);
					if (oldest != NULL &&
					    oldest->shard != my_shard) {
						sleepy = oldest;
					}
				}
			}
		}

		if (sleepy != NULL) {
			release_sleepy(wait_list, sleepy);

			if (cb != NULL) {
				cb(sleepy->private, arg);
			}

			if (wait_list->served != NULL) {
				wait_list->served[sleepy->shard]++;
			}
		}
	}

	mm_spinlock_unlock(&wait_list->lock);

	if (sleepy != NULL) {
		int event_mgr_fd;
		event_mgr_fd = wakeup_event_from_sleepy(sleepy);
		if (event_mgr_fd > 0) {
			mm_eventmgr_wakeup(event_mgr_fd);
		}
		return sleepy->private;
	}

	return NULL;
}

void *mm_wait_list_notify(mm_wait_list_t *wait_list)
{
	return mm_wait_list_notify_cb(wait_list, NULL, NULL);
}

int mm_wait_list_notify_all(mm_wait_list_t *wait_list)
{
	int signaled = 0;

	mm_spinlock_lock(&wait_list->lock);

	uint64_t count = atomic_load(&wait_list->sleepy_count);

	int *event_mgr_fds = mm_malloc(sizeof(int) * count);
	if (event_mgr_fds == NULL) {
		abort();
	}

	if (count > 0) {
		signaled = 1;
	}

	mm_sleepy_t *sleepy;
	for (uint64_t i = 0; i < count; ++i) {
		sleepy = wl_peek_oldest(wait_list);

		release_sleepy(wait_list, sleepy);

		event_mgr_fds[i] = wakeup_event_from_sleepy(sleepy);
	}

	mm_spinlock_unlock(&wait_list->lock);

	for (uint64_t i = 0; i < count; ++i) {
		if (event_mgr_fds[i] > 0) {
			mm_eventmgr_wakeup(event_mgr_fds[i]);
		}
	}

	mm_free(event_mgr_fds);

	return signaled;
}

int mm_wait_list_waitv(mm_wait_list_t **wait_lists, size_t count,
		       uint32_t timeout_ms)
{
	if (count == 0 || wait_lists == NULL) {
		mm_errno_set(EINVAL);
		return -1;
	}

	/* prepare shared event for this waiter */
	mm_event_t shared_event;

	mm_sleepy_t *sleepy_list = mm_malloc(count * sizeof(mm_sleepy_t));
	if (sleepy_list == NULL) {
		return -1;
	}

	for (size_t i = 0; i < count; ++i) {
		mm_spinlock_lock(&wait_lists[i]->lock);
	}

	/* register listeners on all provided wait_lists */
	for (size_t i = 0; i < count; ++i) {
		init_sleepy(&sleepy_list[i], NULL, &shared_event);
		add_sleepy(wait_lists[i], &sleepy_list[i]);
	}
	mm_eventmgr_add(&mm_self->event_mgr, &shared_event);

	for (size_t i = count; i > 0; --i) {
		mm_spinlock_unlock(&wait_lists[i - 1]->lock);
	}

	/* wait for any listener to signal shared_event */
	(void)mm_eventmgr_wait(&mm_self->event_mgr, &shared_event, timeout_ms);

	for (size_t i = count; i > 0; --i) {
		release_sleepy_with_lock(wait_lists[i - 1],
					 &sleepy_list[i - 1]);
	}
	mm_free(sleepy_list);

	int status = shared_event.call.status;
	mm_errno_set(status);
	if (status != 0) {
		return -1;
	}
	return 0;
}
