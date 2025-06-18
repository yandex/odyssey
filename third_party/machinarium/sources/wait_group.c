/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium.h>
#include <machinarium_private.h>

mm_wait_group_t *mm_wait_group_create()
{
	mm_wait_group_t *group = malloc(sizeof(mm_wait_group_t));
	if (group == NULL) {
		return NULL;
	}

	mm_wait_list_t *waiters = mm_wait_list_create(&group->counter);
	if (waiters == NULL) {
		free(group);
		return NULL;
	}
	group->waiters = waiters;

	atomic_init(&group->counter, 0ULL);
	atomic_init(&group->link_count, 1);

	return group;
}

static inline void mm_wait_group_destroy_now(mm_wait_group_t *group)
{
	mm_wait_list_destroy(group->waiters);
	free(group);
}

static inline void mm_wait_group_link(mm_wait_group_t *group)
{
	atomic_fetch_add(&group->link_count, 1);
}

static inline void mm_wait_group_unlink(mm_wait_group_t *group)
{
	if (atomic_fetch_sub(&group->link_count, 1) == 1) {
		mm_wait_group_destroy_now(group);
	}
}

void mm_wait_group_destroy(mm_wait_group_t *group)
{
	mm_wait_group_unlink(group);
}

void mm_wait_group_add(mm_wait_group_t *group)
{
	atomic_fetch_add(&group->counter, 1);
}

uint64_t mm_wait_group_count(mm_wait_group_t *group)
{
	return atomic_load(&group->counter);
}

void mm_wait_group_done(mm_wait_group_t *group)
{
	mm_wait_group_link(group);

	uint64_t old_counter = atomic_fetch_sub(&group->counter, 1);
	if (old_counter == 0ULL) {
		// the counter should not become negative
		abort();
	}
	if (old_counter == 1ULL) {
		mm_wait_list_notify_all(group->waiters);
	}

	mm_wait_group_unlink(group);
}

int mm_wait_group_wait(mm_wait_group_t *group, uint32_t timeout_ms)
{
	uint64_t start_ms = machine_time_ms();
	do {
		uint64_t old_counter = atomic_load(&group->counter);
		if (old_counter == 0) {
			return 0;
		}

		int rc;
		rc = mm_wait_list_compare_wait(group->waiters, old_counter,
					       timeout_ms);
		if (rc == MACHINE_WAIT_LIST_ERR_TIMEOUT_OR_CANCEL) {
			return 1;
		}
	} while (machine_time_ms() - start_ms < timeout_ms);

	return 1;
}

MACHINE_API machine_wait_group_t *machine_wait_group_create()
{
	mm_wait_group_t *group;
	group = mm_wait_group_create();
	if (group == NULL) {
		return NULL;
	}

	return mm_cast(machine_wait_group_t *, group);
}

MACHINE_API void machine_wait_group_destroy(machine_wait_group_t *group)
{
	mm_wait_group_t *wg;
	wg = mm_cast(mm_wait_group_t *, group);

	mm_wait_group_destroy(wg);
}

MACHINE_API void machine_wait_group_add(machine_wait_group_t *group)
{
	mm_wait_group_t *wg;
	wg = mm_cast(mm_wait_group_t *, group);

	mm_wait_group_add(wg);
}

MACHINE_API uint64_t machine_wait_group_count(machine_wait_group_t *group)
{
	mm_wait_group_t *wg;
	wg = mm_cast(mm_wait_group_t *, group);

	return mm_wait_group_count(wg);
}

MACHINE_API void machine_wait_group_done(machine_wait_group_t *group)
{
	mm_wait_group_t *wg;
	wg = mm_cast(mm_wait_group_t *, group);

	mm_wait_group_done(wg);
}

MACHINE_API int machine_wait_group_wait(machine_wait_group_t *group,
					uint32_t timeout_ms)
{
	mm_wait_group_t *wg;
	wg = mm_cast(mm_wait_group_t *, group);

	return mm_wait_group_wait(wg, timeout_ms);
}