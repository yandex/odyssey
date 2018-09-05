
#include <machinarium.h>
#include <odyssey_test.h>
#include <unistd.h>

void
machinarium_test_config(void)
{
	machinarium_set_pool_size(1);
	machinarium_init();
	uint64_t count_machine = 0;
	uint64_t count_coroutine = 0;
	uint64_t count_coroutine_cache = 0;
	uint64_t msg_allocated = 0;
	uint64_t msg_cache_count = 0;
	uint64_t msg_cache_gc_count = 0;
	uint64_t msg_cache_size = 0;
	machinarium_stat(&count_machine, &count_coroutine,
	                 &count_coroutine_cache,
	                 &msg_allocated,
	                 &msg_cache_count,
	                 &msg_cache_gc_count,
	                 &msg_cache_size);
	test(count_machine == 1);
	machinarium_free();

	machinarium_set_pool_size(3);
}
