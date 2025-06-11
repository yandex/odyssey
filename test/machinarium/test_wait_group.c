#include <machinarium.h>
#include <odyssey_test.h>
#include <stdatomic.h>

atomic_uint_fast64_t done_count = 0;

typedef struct {
	machine_wait_group_t *group;
	int num;
} doner_arg_t;

static inline void test_doner(void *arg)
{
	doner_arg_t *darg = arg;

	machine_wait_group_t *group = darg->group;

	machine_sleep(darg->num * 50);

	atomic_fetch_add(&done_count, 1ULL);

	machine_wait_group_done(group);
}

static inline void test_wait_group_simple(void *arg)
{
	(void)arg;

	machine_wait_group_t *group = machine_wait_group_create();

	doner_arg_t arg1 = { .group = group, .num = 1 };
	machine_wait_group_add(group);
	int waiter1 = machine_coroutine_create(test_doner, &arg1);
	test(waiter1 != -1);

	doner_arg_t arg2 = { .group = group, .num = 2 };
	machine_wait_group_add(group);
	int waiter2 = machine_coroutine_create(test_doner, &arg2);
	test(waiter2 != -1);

	doner_arg_t arg3 = { .group = group, .num = 3 };
	machine_wait_group_add(group);
	int waiter3 = machine_coroutine_create(test_doner, &arg3);
	test(waiter3 != -1);

	test(machine_wait_group_count(group) == 3);
	test(machine_wait_group_wait(group, 300) == 0);
	test(atomic_load(&done_count) == 3);
	test(machine_wait_group_count(group) == 0);

	machine_wait_group_destroy(group);
}

static inline void done_loop_thread(void *arg)
{
	machine_wait_group_t *group = arg;

	for (int i = 0; i < 1000000; ++i) {
		machine_wait_group_done(group);
	}
}

static inline void test_wait_group_loop_done(void *arg)
{
	(void)arg;

	machine_wait_group_t *group = machine_wait_group_create();

	machine_wait_group_add(group);
	int id1 = machine_create("done_thread1", done_loop_thread, group);
	test(id1 != -1);

	machine_wait_group_add(group);
	int id2 = machine_create("done_thread2", done_loop_thread, group);
	test(id2 != -1);

	machine_wait_group_add(group);
	int id3 = machine_create("done_thread3", done_loop_thread, group);
	test(id3 != -1);

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
	atomic_init(&done_count, 0);

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