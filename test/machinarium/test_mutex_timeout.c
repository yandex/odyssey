
#include <unistd.h>
#include <machinarium.h>
#include <sleep_lock.h>
#include <odyssey_test.h>

static inline void slow_task(void *arg)
{
	machine_mutex_t *mutex = arg;

	test(machine_mutex_lock(mutex, 100) == 1);

	machine_sleep(1000);

	machine_mutex_unlock(mutex);
}

static inline void timeouted_task(void *arg)
{
	/* ensure slow task held mutex */
	machine_sleep(100);

	machine_mutex_t *mutex = arg;

	test(machine_mutex_lock(mutex, 500) == 0);
}

static inline void test_mutex_timeout(void *a)
{
	(void)a;

	machine_mutex_t mutex;
	machine_mutex_init(&mutex);

	int id1 = machine_create("slow", slow_task, &mutex);
	test(id1 != -1);

	int id2 = machine_create("timeouted_task", timeouted_task, &mutex);
	test(id2 != -1);

	test(machine_wait(id2) != -1);
	test(machine_wait(id1) != -1);

	machine_mutex_destroy(&mutex);
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
