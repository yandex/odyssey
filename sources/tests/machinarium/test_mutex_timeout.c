
#include <unistd.h>
#include <machinarium/machinarium.h>
#include <machinarium/sleep_lock.h>
#include <machinarium/mutex.h>
#include <tests/odyssey_test.h>

static inline void slow_task(void *arg)
{
	mm_mutex_t *mutex = arg;

	test(mm_mutex_lock(mutex, 100) == 1);

	machine_sleep(1000);

	mm_mutex_unlock(mutex);
}

static inline void timeouted_task(void *arg)
{
	/* ensure slow task held mutex */
	machine_sleep(100);

	mm_mutex_t *mutex = arg;

	test(mm_mutex_lock(mutex, 500) == 0);
}

static inline void test_mutex_timeout(void *a)
{
	(void)a;

	mm_mutex_t *mutex = mm_mutex_create();
	test(mutex != NULL);

	int id1 = machine_create("slow", slow_task, mutex);
	test(id1 != -1);

	int id2 = machine_create("timeouted_task", timeouted_task, mutex);
	test(id2 != -1);

	test(machine_wait(id2) != -1);
	test(machine_wait(id1) != -1);

	mm_mutex_free(mutex);
}

void machinarium_test_mutex_timeout(void)
{
	machinarium_init();

	int id;
	id = machine_create("test_mutex_timeout", test_mutex_timeout, NULL);
	test(id != -1);
	test(machine_wait(id) != -1);

	machinarium_free();
}
