
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

static int machinarium_stack_size = 0;
static int machinarium_pool_size = 0;
static int machinarium_coroutine_cache_size = 0;
static int machinarium_initialized = 0;
mm_t       machinarium;

static inline size_t
machinarium_page_size(void)
{
	return sysconf(_SC_PAGESIZE);
}

MACHINE_API void
machinarium_set_stack_size(int size)
{
	machinarium_stack_size = size;
}

MACHINE_API void
machinarium_set_pool_size(int size)
{
	machinarium_pool_size = size;
}

MACHINE_API void
machinarium_set_coroutine_cache_size(int size)
{
	machinarium_coroutine_cache_size = size;
}

MACHINE_API int
machinarium_init(void)
{
	mm_machinemgr_init(&machinarium.machine_mgr);
	mm_msgcache_init(&machinarium.msg_cache);
	/* set default configuration, if not preset */
	if (machinarium_stack_size <= 0)
		machinarium_stack_size = 4;
	if (machinarium_coroutine_cache_size == 0)
		machinarium_coroutine_cache_size = 32;
	if (machinarium_pool_size == 0)
		machinarium_pool_size = 1;
	size_t page_size;
	page_size = machinarium_page_size();
	size_t coroutine_stack_size;
	coroutine_stack_size = machinarium_stack_size * page_size;
	mm_coroutine_cache_init(&machinarium.coroutine_cache,
	                        coroutine_stack_size,
	                        page_size,
	                        machinarium_coroutine_cache_size);
	mm_tls_init();
	mm_taskmgr_init(&machinarium.task_mgr);
	mm_taskmgr_start(&machinarium.task_mgr, machinarium_pool_size);
	machinarium_initialized = 1;
	return 0;
}

MACHINE_API void
machinarium_free(void)
{
	if (! machinarium_initialized)
		return;
	mm_taskmgr_stop(&machinarium.task_mgr);
	mm_machinemgr_free(&machinarium.machine_mgr);
	mm_msgcache_free(&machinarium.msg_cache);
	mm_coroutine_cache_free(&machinarium.coroutine_cache);
	mm_tls_free();
}

MACHINE_API void
machinarium_stat(int *count_machine, int *count_coroutine,
                 int *count_coroutine_cache)
{
	*count_machine = mm_machinemgr_count(&machinarium.machine_mgr);

	mm_coroutine_cache_stat(&machinarium.coroutine_cache,
	                        count_coroutine,
	                        count_coroutine_cache);
}
