/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium.h>
#include <machinarium_private.h>

#if defined(__x86_64__) || defined(__i386) || defined(_X86_)
#define MM_MUTEX_BACKOFF __asm__("pause")
#else
#define MM_MUTEX_BACKOFF
#endif

#define MAX_SPIN_COUNT 100

enum {
	MM_MUTEX_UNLOCKED = 0,
	MM_MUTEX_LOCKED = 1,
};

static inline void check_machinarium_presence()
{
	if (mm_self == NULL || mm_self->scheduler.current == NULL) {
		/* do not use mm_mutex_t outside the machinarium */
		abort();
	}
}

static inline void init_owner(mm_mutex_owner_t *owner)
{
	check_machinarium_presence();
	owner->machine = mm_self;
	owner->coroutine = mm_scheduler_current(&mm_self->scheduler);
	mm_list_init(&owner->link);
}

static inline uint64_t get_current_coro_id()
{
	check_machinarium_presence();
	return mm_self->scheduler.current->id;
}

mm_mutex_t *mm_mutex_create()
{
	mm_mutex_t *mutex = mm_malloc(sizeof(mm_mutex_t));
	if (mutex != NULL) {
		mm_list_init(&mutex->queue);
		atomic_init(&mutex->queue_size, 0);
		mm_sleeplock_init(&mutex->queue_lock);
		atomic_init(&mutex->state, MM_MUTEX_UNLOCKED);
	}

	return mutex;
}

void mm_mutex_destroy(mm_mutex_t *mutex)
{
	/* TODO: maybe handle not empty queue somehow? */
	mm_free(mutex);
}

static inline int mm_mutex_try_lock_fast(mm_mutex_t *mutex)
{
	mm_sleeplock_lock(&mutex->queue_lock);
	uint64_t queue_size = mutex->queue_size;
	mm_sleeplock_unlock(&mutex->queue_lock);

	if (queue_size > 0) {
		return 0;
	}

	for (int i = 0; i < MAX_SPIN_COUNT; ++i) {
		if (atomic_exchange(&mutex->state, MM_MUTEX_LOCKED) ==
		    MM_MUTEX_UNLOCKED) {
			mm_mutex_owner_t owner;
			init_owner(&owner);

			mutex->owner.machine = owner.machine;
			mutex->owner.coroutine = owner.coroutine;

			return 1;
		}
	}

	return 0;
}

static inline int mm_mutex_lock_slow_attempt(mm_mutex_t *mutex,
					     mm_mutex_owner_t *owner,
					     uint32_t timeout_ms)
{
	mm_sleeplock_lock(&mutex->queue_lock);
	{
		mm_eventmgr_add(&mm_self->event_mgr, &owner->event);
		mm_list_append(&mutex->queue, &owner->link);
		atomic_fetch_add(&mutex->queue_size, 1);
	}
	mm_sleeplock_unlock(&mutex->queue_lock);

	mm_eventmgr_wait(&mm_self->event_mgr, &owner->event, timeout_ms);

	mm_sleeplock_lock(&mutex->queue_lock);
	{
		/* timeout or cancel */
		if (owner->event.call.status != 0) {
			--mutex->queue_size;
			mm_list_unlink(&owner->link);
			mm_sleeplock_unlock(&mutex->queue_lock);
			return 0;
		}
	}
	mm_sleeplock_unlock(&mutex->queue_lock);

	/* someone released lock, lets try to acquire */
	if (atomic_exchange(&mutex->state, MM_MUTEX_LOCKED) ==
	    MM_MUTEX_UNLOCKED) {
		return 1;
	}

	return 0;
}

static inline int mm_mutex_lock_slow(mm_mutex_t *mutex, uint32_t timeout_ms)
{
	mm_mutex_owner_t queued_owner;
	init_owner(&queued_owner);

	uint64_t start_ms = machine_time_ms();

	do {
		if (mm_mutex_lock_slow_attempt(mutex, &queued_owner,
					       timeout_ms)) {
			mutex->owner.machine = queued_owner.machine;
			mutex->owner.coroutine = queued_owner.coroutine;
			return 1;
		}
	} while (machine_time_ms() - start_ms < timeout_ms);

	return 0;
}

int mm_mutex_lock(mm_mutex_t *mutex, uint32_t timeout_ms)
{
	if (mm_mutex_try_lock_fast(mutex)) {
		return 1;
	}

	return mm_mutex_lock_slow(mutex, timeout_ms);
}

void mm_mutex_unlock(mm_mutex_t *mutex)
{
	int event_mgr_fd = 0;

	atomic_store(&mutex->state, MM_MUTEX_UNLOCKED);

	mm_sleeplock_lock(&mutex->queue_lock);
	{
		if (mutex->queue_size > 0) {
			mm_mutex_owner_t *queued_owner;
			queued_owner =
				mm_list_peek(mutex->queue, mm_mutex_owner_t);
			mm_list_unlink(&queued_owner->link);
			--mutex->queue_size;
			event_mgr_fd = mm_eventmgr_signal(&queued_owner->event);
		}
	}
	mm_sleeplock_unlock(&mutex->queue_lock);

	if (event_mgr_fd > 0) {
		mm_eventmgr_wakeup(event_mgr_fd);
	}
}

MACHINE_API machine_mutex_t *machine_mutex_create()
{
	mm_mutex_t *mu = mm_mutex_create();
	if (mu != NULL) {
		return mm_cast(machine_mutex_t *, mu);
	}

	return NULL;
}

MACHINE_API void machine_mutex_destroy(machine_mutex_t *mutex)
{
	mm_mutex_t *mu;
	mu = mm_cast(mm_mutex_t *, mutex);

	mm_mutex_destroy(mu);
}

MACHINE_API int machine_mutex_lock(machine_mutex_t *mutex, uint32_t timeout_ms)
{
	mm_mutex_t *mu;
	mu = mm_cast(mm_mutex_t *, mutex);

	return mm_mutex_lock(mu, timeout_ms);
}

MACHINE_API void machine_mutex_unlock(machine_mutex_t *mutex)
{
	mm_mutex_t *mu;
	mu = mm_cast(mm_mutex_t *, mutex);

	mm_mutex_unlock(mu);
}
