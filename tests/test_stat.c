
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

static void
coroutine(void *arg)
{
	machine_sleep(100);
}

void
test_stat(void)
{
	machinarium_init();

	int count_machine = 0;
	int count_coroutine = 0;
	int count_coroutine_cache = 0;

	machinarium_stat(&count_machine, &count_coroutine,
	                 &count_coroutine_cache);

	test(count_machine == 3); /* thread pool */

	int id;
	id = machine_create("test", coroutine, NULL);
	test(id != -1);

	machinarium_stat(&count_machine, &count_coroutine,
	                 &count_coroutine_cache);

	test(count_machine == 4);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_stat(&count_machine, &count_coroutine,
	                 &count_coroutine_cache);

	test(count_machine == 3);

	machinarium_free();
}
