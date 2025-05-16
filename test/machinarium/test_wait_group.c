#include <machinarium.h>
#include <odyssey_test.h>

uint64_t done_count = 0;

typedef struct {
	machine_wait_group_t *group;
	int num;
} doner_arg_t;

static inline uint64_t atomic_inc(uint64_t *ptr, uint64_t v)
{
	return __sync_fetch_and_add(ptr, v);
}

static inline uint64_t atomic_get(uint64_t *ptr)
{
	return atomic_inc(ptr, 0ULL);
}

static inline void test_doner(void *arg)
{
	doner_arg_t *darg = arg;

	machine_wait_group_t *group = darg->group;

	machine_sleep(darg->num * 50);

	atomic_inc(&done_count, 1ULL);

	machine_wait_group_done(group);
}

static inline void test_wait_group_simple(void *arg)
{
	(void)arg;

	machine_wait_group_t *group = machine_wait_group_create();

	doner_arg_t arg1 = { .group = group, .num = 1 };
	int waiter1 = machine_coroutine_create(test_doner, &arg1);
	test(waiter1 != -1);
	machine_wait_group_add(group);

	doner_arg_t arg2 = { .group = group, .num = 2 };
	int waiter2 = machine_coroutine_create(test_doner, &arg2);
	test(waiter2 != -1);
	machine_wait_group_add(group);

	doner_arg_t arg3 = { .group = group, .num = 3 };
	int waiter3 = machine_coroutine_create(test_doner, &arg3);
	test(waiter3 != -1);
	machine_wait_group_add(group);

	test(machine_wait_group_count(group) == 3);
	test(machine_wait_group_wait(group, 300) == 0);
	test(atomic_get(&done_count) == 3);
	test(machine_wait_group_count(group) == 0);

	machine_wait_group_destroy(group);
}

static inline void done_loop_thread(void *arg)
{
	machine_wait_group_t *group = arg;

	machine_sleep(10);

	for (int i = 0; i < 1000000; ++i) {
		machine_wait_group_done(group);
	}
}

static inline void test_wait_group_loop_done(void *arg)
{
	(void)arg;

	machine_wait_group_t *group = machine_wait_group_create();

	int id1 = machine_create("done_thread1", done_loop_thread, group);
	test(id1 != -1);

	int id2 = machine_create("done_thread2", done_loop_thread, group);
	test(id2 != -1);

	int id3 = machine_create("done_thread3", done_loop_thread, group);
	test(id3 != -1);

	machine_sleep(5);
	machine_wait_group_add(group);

	machine_sleep(5);
	machine_wait_group_add(group);

	machine_sleep(5);
	machine_wait_group_add(group);

	int rc;
	rc = machine_wait(id1);
	test(rc != -1);

	rc = machine_wait(id2);
	test(rc != -1);

	rc = machine_wait(id3);
	test(rc != -1);

	test(machine_wait_group_count(group) == 0ULL);

	machine_wait_group_destroy(group);
}

void machinarium_test_wait_group()
{
	machinarium_init();

	int id;
	id = machine_create("machinarium_test_wait_group_simple",
			    test_wait_group_simple, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	id = machine_create("machinarium_test_wait_group_loop_done",
			    test_wait_group_loop_done, NULL);
	test(id != -1);

	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}