#include <machinarium/machinarium.h>
#include <machinarium/wait_flag.h>
#include <tests/odyssey_test.h>

static inline void test_wait_flag_timeout(void *arg)
{
	(void)arg;

	mm_wait_flag_t flag;
	mm_wait_flag_init(&flag);

	/* nobody sets the flag */

	int rc = mm_wait_flag_wait(&flag, 10);
	test(rc == -1);
	test(machine_errno() == ETIMEDOUT);

	mm_wait_flag_destroy(&flag);
}

void machinarium_test_wait_flag_timeout(void)
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
