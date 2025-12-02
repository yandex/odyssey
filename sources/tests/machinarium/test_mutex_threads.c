
#include <unistd.h>
#include <machinarium/machinarium.h>
#include <machinarium/sleep_lock.h>
#include <tests/odyssey_test.h>

typedef struct {
	uint64_t *counter;
	machine_mutex_t *mutex;
	machine_wait_group_t *wg;
} incrementer_arg_t;

static inline void incrementer(void *arg)
{
	incrementer_arg_t *iarg = arg;

	for (int i = 0; i < (1 << 16); ++i) {
		test(machine_mutex_lock(iarg->mutex, 1000) == 1);
		(*iarg->counter)++;
		machine_mutex_unlock(iarg->mutex);
	}

	machine_wait_group_done(iarg->wg);
}

static inline void test_threads_access(void *a)
{
	(void)a;

	uint64_t counter = 0;
	machine_mutex_t *mutex = machine_mutex_create();
	test(mutex != NULL);

	machine_wait_group_t *wg = machine_wait_group_create();
	test(wg != NULL);

	incrementer_arg_t arg = { .counter = &counter,
				  .mutex = mutex,
				  .wg = wg };

	machine_wait_group_add(wg);
	int id1 = machine_create("test1", incrementer, &arg);
	test(id1 != -1);

	machine_wait_group_add(wg);
	int id2 = machine_create("test2", incrementer, &arg);
	test(id2 != -1);

	machine_wait_group_add(wg);
	int id3 = machine_create("test3", incrementer, &arg);
	test(id3 != -1);

	machine_wait_group_add(wg);
	int id4 = machine_create("test4", incrementer, &arg);
	test(id4 != -1);

	test(machine_wait_group_wait(wg, UINT32_MAX) == 0);

	test(counter == (4L << 16));

	machine_mutex_destroy(mutex);
}

void machinarium_test_mutex_threads(void)
{
	machinarium_init();

	int id;
	id = machine_create("test_mutex_threads_access", test_threads_access,
			    NULL);
	test(id != -1);
	test(machine_wait(id) != -1);

	machinarium_free();
}
