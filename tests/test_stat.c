
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>
#include <unistd.h>

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

	for (;;) {
		machinarium_stat(&count_machine, &count_coroutine,
		                 &count_coroutine_cache);
		test(count_machine == 3); /* thread pool */
		test(count_coroutine_cache == 0);
		if (count_coroutine != 3) {
			usleep(10000);
			continue;
		}
		break;
	}

	int id;
	id = machine_create("test", coroutine, NULL);
	test(id != -1);

	for (;;) {
		machinarium_stat(&count_machine, &count_coroutine,
		                 &count_coroutine_cache);
		test(count_machine == 3 + 1);
		test(count_coroutine_cache == 0);
		if (count_coroutine != 4) {
			usleep(10000);
			continue;
		}
		break;
	}

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	for (;;) {
		machinarium_stat(&count_machine, &count_coroutine,
		                 &count_coroutine_cache);
		test(count_machine == 3)
		if (count_coroutine != 3) {
			usleep(10000);
			continue;
		}
		test(count_coroutine_cache == 1);
		break;
	}

	machinarium_free();
}
