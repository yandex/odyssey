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

mm_wait_flag_t *mm_wait_flag_create(void)
{
	mm_wait_flag_t *flag = mm_malloc(sizeof(mm_wait_flag_t));
	if (flag == NULL) {
		return NULL;
	}

	mm_wait_list_t *waiters = mm_wait_list_create(&flag->value);
	if (waiters == NULL) {
		mm_free(flag);
		return NULL;
	}
	flag->waiters = waiters;

	atomic_init(&flag->value, 0);
	atomic_init(&flag->link_count, 1);

	return flag;
}

static inline void mm_wait_flag_destroy_now(mm_wait_flag_t *flag)
{
	mm_wait_list_destroy(flag->waiters);
	mm_free(flag);
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

		int rc = mm_wait_list_compare_wait(flag->waiters, value,
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
		mm_wait_list_notify_all(flag->waiters);
	}
	mm_wait_flag_unlink(flag);
}

MACHINE_API machine_wait_flag_t *machine_wait_flag_create(void)
{
	mm_wait_flag_t *flag;
	flag = mm_wait_flag_create();
	if (flag == NULL) {
		return NULL;
	}

	return mm_cast(machine_wait_flag_t *, flag);
}

MACHINE_API void machine_wait_flag_destroy(machine_wait_flag_t *flag)
{
	mm_wait_flag_t *flag_;
	flag_ = mm_cast(mm_wait_flag_t *, flag);

	mm_wait_flag_destroy(flag_);
}

MACHINE_API int machine_wait_flag_wait(machine_wait_flag_t *flag,
				       uint32_t timeout_ms)
{
	mm_wait_flag_t *flag_;
	flag_ = mm_cast(mm_wait_flag_t *, flag);

	return mm_wait_flag_wait(flag_, timeout_ms);
}

MACHINE_API void machine_wait_flag_set(machine_wait_flag_t *flag)
{
	mm_wait_flag_t *flag_;
	flag_ = mm_cast(mm_wait_flag_t *, flag);

	mm_wait_flag_set(flag_);
}
