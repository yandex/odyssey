#include <machinarium.h>
#include <tests/odyssey_test.h>
#include <stdatomic.h>

atomic_uint_fast64_t flag;

typedef struct {
	machine_wait_flag_t *flag;
	machine_wait_group_t *group;

	/* only for consumer */
	uint32_t estimated_wait_time_lower_bound;
	uint32_t estimated_wait_time_upper_bound;
} doner_arg_t;

static inline void consumer(void *arg)
{
	doner_arg_t *darg = arg;

	uint32_t start_time = machine_time_ms();
	test(machine_wait_flag_wait(darg->flag, UINT32_MAX) == 0);
	uint32_t wait_time = machine_time_ms() - start_time;
	test(wait_time >= darg->estimated_wait_time_lower_bound);
	test(darg->estimated_wait_time_upper_bound >= wait_time);
	test(atomic_load(&flag) == 1);
	machine_wait_group_done(darg->group);
}

static inline void producer(void *arg)
{
	doner_arg_t *darg = arg;
	atomic_store(&flag, 1);
	machine_wait_flag_set(darg->flag);
	machine_wait_group_done(darg->group);
}

static inline void test_wait_flag_simple_coroutines(void *arg)
{
	(void)arg;

	machine_wait_flag_t *flag = machine_wait_flag_create();

	machine_wait_group_t *group = machine_wait_group_create();

	/* wait before set */
	doner_arg_t arg1 = { .flag = flag,
			     .group = group,
			     .estimated_wait_time_lower_bound = 100,
			     .estimated_wait_time_upper_bound = 105 };
	machine_wait_group_add(group);
	int id1 = machine_coroutine_create(consumer, &arg1);
	test(id1 != -1);

	machine_sleep(100);

	doner_arg_t arg2 = { .flag = flag, .group = group };
	machine_wait_group_add(group);
	int id2 = machine_coroutine_create(producer, &arg2);
	test(id2 != -1);

	machine_sleep(100);

	/* wait after set */
	doner_arg_t arg3 = { .flag = flag,
			     .group = group,
			     .estimated_wait_time_lower_bound = 0,
			     .estimated_wait_time_upper_bound = 5 };
	machine_wait_group_add(group);
	int id3 = machine_coroutine_create(consumer, &arg3);
	test(id3 != -1);

	machine_sleep(100);

	/* second set */
	doner_arg_t arg4 = { .flag = flag, .group = group };
	machine_wait_group_add(group);
	int id4 = machine_coroutine_create(producer, &arg4);
	test(id4 != -1);

	machine_wait_group_wait(group, 1000);

	machine_wait_flag_destroy(flag);
}

static inline void test_wait_flag_simple_threads(void *arg)
{
	(void)arg;

	machine_wait_flag_t *flag = machine_wait_flag_create();

	machine_wait_group_t *group = machine_wait_group_create();

	/* wait before set */
	doner_arg_t arg1 = { .flag = flag,
			     .group = group,
			     .estimated_wait_time_lower_bound = 100,
			     .estimated_wait_time_upper_bound = 105 };
	machine_wait_group_add(group);
	int id1 = machine_create("test_wait_flag_consumer_1", consumer, &arg1);
	test(id1 != -1);

	machine_sleep(100);

	doner_arg_t arg2 = { .flag = flag, .group = group };
	machine_wait_group_add(group);
	int id2 = machine_create("test_wait_flag_producer_1", producer, &arg2);
	test(id2 != -1);

	machine_sleep(100);

	/* wait after set */
	doner_arg_t arg3 = { .flag = flag,
			     .group = group,
			     .estimated_wait_time_lower_bound = 0,
			     .estimated_wait_time_upper_bound = 5 };
	machine_wait_group_add(group);
	int id3 = machine_create("test_wait_flag_consumer_2", consumer, &arg3);
	test(id3 != -1);

	machine_sleep(100);

	/* second set */
	doner_arg_t arg4 = { .flag = flag, .group = group };
	machine_wait_group_add(group);
	int id4 = machine_create("test_wait_flag_producer_2", producer, &arg4);
	test(id4 != -1);

	machine_wait_group_wait(group, 1000);

	test(machine_wait(id1) != -1);
	test(machine_wait(id2) != -1);
	test(machine_wait(id3) != -1);
	test(machine_wait(id4) != -1);

	machine_wait_flag_destroy(flag);
}

void machinarium_test_wait_flag_simple()
{
	machinarium_init();

	atomic_init(&flag, 0);

	int id;
	id = machine_create("machinarium_test_wait_flag_simple_coroutines",
			    test_wait_flag_simple_coroutines, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	atomic_store(&flag, 0);

	id = machine_create("machinarium_test_wait_flag_simple_threads",
			    test_wait_flag_simple_threads, NULL);
	test(id != -1);

	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}
