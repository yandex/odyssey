/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>
#include <stdint.h>

#include <machinarium/machinarium.h>
#include <machinarium/machine.h>
#include <machinarium/sem.h>

void mm_sem_init(mm_sem_t *sem, uint64_t value)
{
	atomic_init(&sem->value, value);
	mm_wait_list_init(&sem->notifier, &sem->value);
}

void mm_sem_destroy(mm_sem_t *sem)
{
	mm_wait_list_destroy(&sem->notifier);
}

int mm_sem_wait(mm_sem_t *sem)
{
	return mm_sem_timedwait(sem, UINT32_MAX);
}

int mm_sem_timedwait(mm_sem_t *sem, uint32_t timeout_ms)
{
	uint64_t start_ms = machine_time_ms();
	uint64_t end_ms = start_ms + timeout_ms;
	int infinite = timeout_ms == UINT32_MAX;

	while (1) {
		uint64_t val = atomic_load(&sem->value);

		if (val == 0) {
			uint64_t now_ms = machine_time_ms();

			if (!infinite && now_ms >= end_ms) {
				mm_errno_set(ETIMEDOUT);
				return -1;
			}

			uint64_t left = infinite ? UINT32_MAX : end_ms - now_ms;

			int rc = mm_wait_list_compare_wait(&sem->notifier, NULL,
							   0, (uint32_t)left);
			if (rc == -1 && mm_errno_get() != EAGAIN) {
				return rc;
			}
			continue;
		}

		if (atomic_compare_exchange_weak(&sem->value, &val, val - 1)) {
			break;
		}
	}

	return 0;
}

void mm_sem_post(mm_sem_t *sem)
{
	uint64_t v = atomic_fetch_add(&sem->value, 1);
	if (mm_unlikely(v == UINT64_MAX)) {
		abort();
	}

	mm_wait_list_notify(&sem->notifier);
}
