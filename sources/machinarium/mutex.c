/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>

#include <machinarium/machinarium.h>
#include <machinarium/machine.h>
#include <machinarium/mutex.h>

enum {
	MM_MUTEX_UNLOCKED = 0,
	MM_MUTEX_LOCKED = 1,
};

void mm_mutex_init(mm_mutex_t *mutex)
{
	atomic_init(&mutex->state, MM_MUTEX_UNLOCKED);
	atomic_init(&mutex->owner_machine, (uintptr_t)NULL);
	atomic_init(&mutex->owner_coro_id, MM_SLEEPY_NO_CORO_ID);

	mm_wait_list_init(&mutex->wl, &mutex->state);
}

mm_mutex_t *mm_mutex_create(void)
{
	mm_mutex_t *mutex = mm_malloc(sizeof(mm_mutex_t));
	if (mutex == NULL) {
		return NULL;
	}

	mm_mutex_init(mutex);

	return mutex;
}

void mm_mutex_destroy(mm_mutex_t *mutex)
{
	/* TODO: maybe handle not empty queue somehow? */
	mm_wait_list_destroy(&mutex->wl);
}

void mm_mutex_free(mm_mutex_t *mutex)
{
	mm_mutex_destroy(mutex);
	mm_free(mutex);
}

int mm_mutex_lock(mm_mutex_t *mutex, uint32_t timeout_ms)
{
	if (mm_self == NULL || mm_self->scheduler.current == NULL) {
		abort();
	}

	uint64_t start_ms = machine_time_ms();

	while (1) {
		uint_fast64_t expected = MM_MUTEX_UNLOCKED;
		if (atomic_compare_exchange_strong(&mutex->state, &expected,
						   MM_MUTEX_LOCKED)) {
			atomic_store(&mutex->owner_machine, (uintptr_t)mm_self);
			atomic_store(&mutex->owner_coro_id,
				     mm_self->scheduler.current->id);
			return 1;
		}

		uint64_t elapsed = machine_time_ms() - start_ms;
		if (elapsed >= timeout_ms) {
			return 0;
		}

		uint32_t remaining = timeout_ms - (uint32_t)elapsed;

		int rc = mm_wait_list_compare_wait(&mutex->wl, MM_MUTEX_LOCKED,
						   remaining);
		if (rc == -1 && mm_errno_get() == ECANCELED) {
			return 0;
		}

		/* all other scenarios leads to retry */
	}

	return 0;
}

void mm_mutex_unlock(mm_mutex_t *mutex)
{
	if (mm_self == NULL || mm_self->scheduler.current == NULL) {
		abort();
	}

	assert(atomic_load(&mutex->owner_machine) == (uintptr_t)mm_self);
	assert(atomic_load(&mutex->owner_coro_id) ==
	       mm_self->scheduler.current->id);

	atomic_store(&mutex->owner_machine, (uintptr_t)NULL);
	atomic_store(&mutex->owner_coro_id, MM_SLEEPY_NO_CORO_ID);
	atomic_store(&mutex->state, MM_MUTEX_UNLOCKED);

	mm_wait_list_notify(&mutex->wl);
}
