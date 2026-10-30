/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium/machinarium.h>
#include <machinarium/machine.h>
#include <machinarium/mutex.h>

void mm_mutex_init(mm_mutex_t *mutex)
{
	mm_sem_init(&mutex->sem, 1);
	mutex->owner_machine = NULL;
	mutex->owner_coro_id = MM_SLEEPY_NO_CORO_ID;
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
	mm_sem_destroy(&mutex->sem);
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

	/* returns 0 on both timeout and cancellation */
	if (mm_sem_timedwait(&mutex->sem, timeout_ms) != 0) {
		return 0;
	}

	mutex->owner_machine = (void *)mm_self;
	mutex->owner_coro_id = mm_self->scheduler.current->id;
	return 1;
}

int mm_mutex_lock2(mm_mutex_t *mutex)
{
	return mm_mutex_lock(mutex, UINT32_MAX);
}

void mm_mutex_unlock(mm_mutex_t *mutex)
{
	if (mm_self == NULL || mm_self->scheduler.current == NULL) {
		abort();
	}

	mutex->owner_machine = NULL;
	mutex->owner_coro_id = MM_SLEEPY_NO_CORO_ID;

	mm_sem_post(&mutex->sem);
}
