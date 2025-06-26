#include <machinarium.h>
#include <odyssey_test.h>
#include <stdatomic.h>

#define NUM_COROUTINES 10

static uint64_t value = 0;

static inline void nop(void *arg)
{
	(void)arg;
}

static inline void producer(void *arg)
{
	int number = *((int *)arg);
	value = number;
}

static inline void spawn_coroutines(void *arg)
{
	size_t producer_index = *((size_t *)(arg));

	int ids[NUM_COROUTINES];
	int args[NUM_COROUTINES];
	for (size_t i = 0; i < NUM_COROUTINES; ++i) {
		args[i] = i;
		if (i == producer_index) {
			ids[i] = machine_coroutine_create(producer, &args[i]);
		} else {
			ids[i] = machine_coroutine_create(nop, &args[i]);
		}
		test(ids[i] != -1);
	}

	for (size_t i = 0; i < NUM_COROUTINES; ++i) {
		machine_join(ids[i]);
	}
}

static inline void test_simple_race_example(void *arg)
{
	(void)arg;

	int id1, id2;
	size_t producer_index_1 = 2;
	id1 = machine_create("test_tsan_simple_race_example_1", spawn_coroutines,
			     &producer_index_1);
	test(id1 != -1);
	size_t producer_index_2 = 4;
	id2 = machine_create("test_tsan_simple_race_example_2", spawn_coroutines,
			     &producer_index_2);
	test(id2 != -1);

	int rc;
	rc = machine_wait(id1);
	test(rc == 0);
	rc = machine_wait(id2);
	test(rc == 0);
}

void machinarium_test_tsan_simple_race_example()
{
	machinarium_init();

	int id;
	id = machine_create(
		"test_wait_list_notify_after_compare_wait_coroutines",
		test_simple_race_example, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}
