#include <machinarium.h>
#include <odyssey_test.h>
#include <stdatomic.h>

atomic_uint_fast64_t done_count;

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

static inline void test_wait_group_simple_coroutines(void *arg)
{
	(void)arg;

	machine_wait_group_t *group = machine_wait_group_create();

	doner_arg_t arg1 = { .group = group, .num = 1 };
	machine_wait_group_add(group);
	int id1 = machine_coroutine_create(test_doner, &arg1);
	test(id1 != -1);

	doner_arg_t arg2 = { .group = group, .num = 2 };
	machine_wait_group_add(group);
	int id2 = machine_coroutine_create(test_doner, &arg2);
	test(id2 != -1);

	doner_arg_t arg3 = { .group = group, .num = 3 };
	machine_wait_group_add(group);
	int id3 = machine_coroutine_create(test_doner, &arg3);
	test(id3 != -1);

	test(machine_wait_group_count(group) == 3);
	test(machine_wait_group_wait(group, 300) == 0);
	test(atomic_load(&done_count) == 3);
	test(machine_wait_group_count(group) == 0);

	machine_wait_group_destroy(group);
}

static inline void test_wait_group_simple_threads(void *arg)
{
	(void)arg;

	machine_wait_group_t *group = machine_wait_group_create();

	doner_arg_t arg1 = { .group = group, .num = 1 };
	machine_wait_group_add(group);
	int id1 = machine_create("done_thread1", test_doner, &arg1);
	test(id1 != -1);

	doner_arg_t arg2 = { .group = group, .num = 2 };
	machine_wait_group_add(group);
	int id2 = machine_create("done_thread2", test_doner, &arg2);
	test(id2 != -1);

	doner_arg_t arg3 = { .group = group, .num = 3 };
	machine_wait_group_add(group);
	int id3 = machine_create("done_thread1", test_doner, &arg3);
	test(id3 != -1);

	test(machine_wait_group_count(group) == 3);
	test(machine_wait_group_wait(group, 300) == 0);
	test(atomic_load(&done_count) == 3);
	test(machine_wait_group_count(group) == 0);

	int rc;
	rc = machine_wait(id1);
	test(rc != -1);

	rc = machine_wait(id2);
	test(rc != -1);

	rc = machine_wait(id3);
	test(rc != -1);

	machine_wait_group_destroy(group);
}

void machinarium_test_wait_group_simple()
{
	machinarium_init();

	atomic_init(&done_count, 0);

	int id;
	id = machine_create("machinarium_test_wait_group_simple_coroutines",
			    test_wait_group_simple_coroutines, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	atomic_store(&done_count, 0);

	id = machine_create("machinarium_test_wait_group_simple_threads",
			    test_wait_group_simple_threads, NULL);
	test(id != -1);

	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}
