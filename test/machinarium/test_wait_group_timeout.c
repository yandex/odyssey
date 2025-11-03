#include <machinarium.h>
#include <odyssey_test.h>

static inline void test_example_coroutine(void *arg)
{
	(void)arg;

	/* do something and forget to call done() */
	machine_sleep(0);
	/* machine_wait_group_t *wl = arg; */
	/* machine_wait_group_done(wl); */
}

static inline void test_wait_group_timeout(void *arg)
{
	(void)arg;

	machine_wait_group_t *group = machine_wait_group_create();

	machine_wait_group_add(group);
	int id;
	id = machine_create("machinarium_test_wait_group_timeout",
			    test_example_coroutine, NULL);
	test(id != -1);

	int rc = machine_wait_group_wait(group, 10);
	test(rc == -1);
	test(machine_errno() == ETIMEDOUT);

	rc = machine_wait(id);
	test(rc != -1);

	machine_wait_group_destroy(group);
}

void machinarium_test_wait_group_timeout()
{
	machinarium_init();

	int id;
	id = machine_create("machinarium_test_wait_group_timeout",
			    test_wait_group_timeout, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}
