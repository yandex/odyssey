
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>
#include <unistd.h>

void
test_config(void)
{
	machinarium_set_pool_size(1);
	machinarium_init();
	int count_machine = 0;
	int count_coroutine = 0;
	int count_coroutine_cache = 0;
	machinarium_stat(&count_machine, &count_coroutine,
	                 &count_coroutine_cache);
	test(count_machine == 1);
	machinarium_free();

	machinarium_set_pool_size(3);
}
