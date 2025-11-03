#include <machinarium.h>
#include <odyssey_test.h>

static inline void test_wait_flag_timeout(void *arg)
{
	(void)arg;

	machine_wait_flag_t *flag = machine_wait_flag_create();

	/* nobody sets the flag */

	int rc = machine_wait_flag_wait(flag, 10);
	test(rc == -1);
	test(machine_errno() == ETIMEDOUT);

	machine_wait_flag_destroy(flag);
}

void machinarium_test_wait_flag_timeout()
{
	machinarium_init();

	int id;
	id = machine_create("machinarium_test_wait_flag_timeout",
			    test_wait_flag_timeout, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}
