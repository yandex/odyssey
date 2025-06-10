/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium.h>
#include <machinarium_private.h>

mm_wait_group_t *mm_wait_group_create()
{
	mm_wait_list_t *waiters = mm_wait_list_create();
	if (waiters == NULL) {
		return NULL;
	}

	mm_wait_group_t *group = malloc(sizeof(mm_wait_group_t));
	if (group == NULL) {
		mm_wait_list_destroy(waiters);
		return NULL;
	}
	memset(group, 0, sizeof(mm_wait_group_t));

	group->waiters = waiters;
	atomic_init(&group->counter, 0ULL);

	return group;
}

void mm_wait_group_destroy(mm_wait_group_t *group)
{
	mm_wait_list_destroy(group->waiters);
	free(group);
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
	uint64_t old_counter = atomic_load(&group->counter);
	for (;;) {
		if (old_counter == 0ULL) {
			return;
		}

		if (atomic_compare_exchange_weak(&group->counter, &old_counter,
						 old_counter - 1)) {
			if (old_counter == 1ULL) {
				mm_wait_list_notify_all(group->waiters);
			}
			return;
		}
	}
}

int mm_wait_group_wait(mm_wait_group_t *group, uint32_t timeout_ms)
{
	if (atomic_load(&group->counter) == 0ULL) {
		return 0;
	}

	return mm_wait_list_wait(group->waiters, timeout_ms);
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