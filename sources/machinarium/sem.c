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
	while (1) {
		uint64_t val = atomic_load(&sem->value);

		if (val == 0) {
			int rc = mm_wait_list_compare_wait(&sem->notifier, NULL,
							   val, UINT32_MAX);
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
