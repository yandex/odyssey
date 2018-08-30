
#include <machinarium.h>
#include <odyssey_test.h>
#include <unistd.h>

void
machinarium_test_config(void)
{
	machinarium_set_pool_size(1);
	machinarium_init();
	int count_machine = 0;
	int count_coroutine = 0;
	int count_coroutine_cache = 0;
	int msg_allocated = 0;
	int msg_cache_count = 0;
	int msg_cache_size = 0;
	machinarium_stat(&count_machine, &count_coroutine,
	                 &count_coroutine_cache,
	                 &msg_allocated,
	                 &msg_cache_count,
	                 &msg_cache_size);
	test(count_machine == 1);
	machinarium_free();

	machinarium_set_pool_size(3);
}
