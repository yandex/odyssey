/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>

#include <machinarium/machinarium.h>
#include <machinarium/wait_flag.h>
#include <machinarium/machine.h>
#include <machinarium/memory.h>

void mm_wait_flag_init(mm_wait_flag_t *flag)
{
	atomic_init(&flag->value, 0);
	atomic_init(&flag->link_count, 1);

	mm_wait_list_init(&flag->waiters, &flag->value);
}

static inline void mm_wait_flag_destroy_now(mm_wait_flag_t *flag)
{
	mm_wait_list_destroy(&flag->waiters);
}

static inline void mm_wait_flag_link(mm_wait_flag_t *flag)
{
	atomic_fetch_add(&flag->link_count, 1);
}

static inline void mm_wait_flag_unlink(mm_wait_flag_t *flag)
{
	if (atomic_fetch_sub(&flag->link_count, 1) == 1) {
		mm_wait_flag_destroy_now(flag);
	}
}

void mm_wait_flag_destroy(mm_wait_flag_t *flag)
{
	mm_wait_flag_unlink(flag);
}

int mm_wait_flag_wait(mm_wait_flag_t *flag, uint32_t timeout_ms)
{
	uint64_t start_ms = machine_time_ms();
	do {
		uint64_t value = atomic_load(&flag->value);
		if (value == 1) {
			return 0;
		}

		int rc = mm_wait_list_compare_wait(&flag->waiters, NULL, value,
						   timeout_ms);
		if (rc != 0 && machine_errno() != EAGAIN) {
			return -1;
		}
	} while (machine_time_ms() - start_ms < timeout_ms);

	mm_errno_set(ETIMEDOUT);
	return -1;
}

void mm_wait_flag_set(mm_wait_flag_t *flag)
{
	mm_wait_flag_link(flag);
	if (atomic_exchange(&flag->value, 1) == 0) {
		mm_wait_list_notify_all(&flag->waiters);
	}
	mm_wait_flag_unlink(flag);
}
