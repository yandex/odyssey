
#include <machinarium.h>
#include <odyssey_test.h>

static void sleeping_worker(void *arg)
{
	(void)arg;
	machine_sleep(100);
}

static void test_wait_fast(void *arg)
{
	(void)arg;

	int64_t id1, id2;
	id1 = machine_create("sleeping_worker1", sleeping_worker, NULL);
	test(id1 != -1);
	id2 = machine_create("sleeping_worker2", sleeping_worker, NULL);
	test(id2 != -1);

	uint64_t start = machine_time_ms();

	int rc;
	rc = machine_wait_fast(id1);
	test(rc == 0);
	rc = machine_wait_fast(id2);
	test(rc == 0);

	uint64_t elapsed = machine_time_ms() - start;
	test(100 <= elapsed && elapsed <= 105);

	machine_stop_current();
}

void machinarium_test_wait_fast(void)
{
	machinarium_init();

	int id;
	id = machine_create("test", test_wait_fast, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}
