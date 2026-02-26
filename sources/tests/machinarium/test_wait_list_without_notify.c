#include <machinarium/machinarium.h>
#include <machinarium/wait_list.h>
#include <tests/odyssey_test.h>

static inline void test_wait_without_notify_coroutine(void *arg)
{
	(void)arg;

	mm_wait_list_t *wait_list = mm_wait_list_create(NULL);

	uint64_t start, end;
	start = machine_time_ms();

	int rc;
	rc = mm_wait_list_wait(wait_list, 1000);
	end = machine_time_ms();
	test(rc == -1);
	test(machine_errno() == ETIMEDOUT);
	test(end - start >= 1000);

	/* notify without waiters should be ignored */
	mm_wait_list_notify(wait_list);
	rc = mm_wait_list_wait(wait_list, 1000);
	test(rc == -1);
	test(machine_errno() == ETIMEDOUT);

	mm_wait_list_free(wait_list);
}

static inline void test_wait_without_notify(void *arg)
{
	(void)arg;

	int id;
	id = machine_coroutine_create(test_wait_without_notify_coroutine, NULL);
	test(id != -1);

	machine_sleep(0);

	int rc;
	rc = machine_join(id);
	test(rc == 0);
}

void machinarium_test_wait_list_without_notify(void)
{
	machinarium_init();

	int id;
	id = machine_create("test_wait_list_wait_without_notify",
			    test_wait_without_notify, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}
