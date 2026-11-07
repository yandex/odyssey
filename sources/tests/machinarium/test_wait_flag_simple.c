#include <machinarium/machinarium.h>
#include <tests/odyssey_test.h>
#include <stdatomic.h>

#define WAIT_TIMEOUT_MS 10000

atomic_uint_fast64_t flag;

typedef struct {
	machine_wait_flag_t *flag;
	machine_wait_group_t *group;

	/* non-NULL for a consumer which must wait before set */
	machine_wait_group_t *ready;
} doner_arg_t;

static void consumer_ready(void *arg)
{
	doner_arg_t *darg = arg;
	test(atomic_load(&flag) == 0);
	machine_wait_group_done(darg->ready);
}

static inline void consumer(void *arg)
{
	doner_arg_t *darg = arg;

	if (darg->ready != NULL) {
		/*
		 * This coroutine runs on the same machine, so it can only
		 * signal readiness after we yield in wait_flag_wait below.
		 * The producer is not started until then.
		 */
		int64_t id = machine_coroutine_create(consumer_ready, darg);
		test(id != -1);
	}

	/* An already-set flag must also succeed with a zero timeout. */
	uint32_t timeout_ms = darg->ready != NULL ? WAIT_TIMEOUT_MS : 0;
	test(machine_wait_flag_wait(darg->flag, timeout_ms) == 0);
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
	test(flag != NULL);

	machine_wait_group_t *group = machine_wait_group_create();
	test(group != NULL);
	machine_wait_group_t *ready = machine_wait_group_create();
	test(ready != NULL);
	machine_wait_group_add(ready);

	/* wait before set */
	doner_arg_t arg1 = { .flag = flag, .group = group, .ready = ready };
	machine_wait_group_add(group);
	int id1 = machine_coroutine_create(consumer, &arg1);
	test(id1 != -1);

	test(machine_wait_group_wait(ready, WAIT_TIMEOUT_MS) == 0);
	test(machine_wait_group_count(group) == 1);

	doner_arg_t arg2 = { .flag = flag, .group = group };
	machine_wait_group_add(group);
	int id2 = machine_coroutine_create(producer, &arg2);
	test(id2 != -1);

	test(machine_wait_group_wait(group, WAIT_TIMEOUT_MS) == 0);

	/* wait after set */
	doner_arg_t arg3 = { .flag = flag, .group = group };
	machine_wait_group_add(group);
	int id3 = machine_coroutine_create(consumer, &arg3);
	test(id3 != -1);

	test(machine_wait_group_wait(group, WAIT_TIMEOUT_MS) == 0);

	/* second set */
	doner_arg_t arg4 = { .flag = flag, .group = group };
	machine_wait_group_add(group);
	int id4 = machine_coroutine_create(producer, &arg4);
	test(id4 != -1);

	test(machine_wait_group_wait(group, WAIT_TIMEOUT_MS) == 0);
	test(machine_wait_flag_wait(flag, 0) == 0);

	machine_wait_group_destroy(ready);
	machine_wait_group_destroy(group);
	machine_wait_flag_destroy(flag);
}

static inline void test_wait_flag_simple_threads(void *arg)
{
	(void)arg;

	machine_wait_flag_t *flag = machine_wait_flag_create();
	test(flag != NULL);

	machine_wait_group_t *group = machine_wait_group_create();
	test(group != NULL);
	machine_wait_group_t *ready = machine_wait_group_create();
	test(ready != NULL);
	machine_wait_group_add(ready);

	/* wait before set */
	doner_arg_t arg1 = { .flag = flag, .group = group, .ready = ready };
	machine_wait_group_add(group);
	int id1 = machine_create("test_wait_flag_consumer_1", consumer, &arg1);
	test(id1 != -1);

	test(machine_wait_group_wait(ready, WAIT_TIMEOUT_MS) == 0);
	test(machine_wait_group_count(group) == 1);

	doner_arg_t arg2 = { .flag = flag, .group = group };
	machine_wait_group_add(group);
	int id2 = machine_create("test_wait_flag_producer_1", producer, &arg2);
	test(id2 != -1);

	test(machine_wait_group_wait(group, WAIT_TIMEOUT_MS) == 0);

	/* wait after set */
	doner_arg_t arg3 = { .flag = flag, .group = group };
	machine_wait_group_add(group);
	int id3 = machine_create("test_wait_flag_consumer_2", consumer, &arg3);
	test(id3 != -1);

	test(machine_wait_group_wait(group, WAIT_TIMEOUT_MS) == 0);

	/* second set */
	doner_arg_t arg4 = { .flag = flag, .group = group };
	machine_wait_group_add(group);
	int id4 = machine_create("test_wait_flag_producer_2", producer, &arg4);
	test(id4 != -1);

	test(machine_wait_group_wait(group, WAIT_TIMEOUT_MS) == 0);
	test(machine_wait_flag_wait(flag, 0) == 0);

	test(machine_wait(id1) != -1);
	test(machine_wait(id2) != -1);
	test(machine_wait(id3) != -1);
	test(machine_wait(id4) != -1);

	machine_wait_group_destroy(ready);
	machine_wait_group_destroy(group);
	machine_wait_flag_destroy(flag);
}

void machinarium_test_wait_flag_simple(void)
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
