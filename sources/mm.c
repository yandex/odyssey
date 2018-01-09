
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

static int machinarium_initialized = 0;
mm_t       machinarium;

static inline size_t
machinarium_page_size(void)
{
	return sysconf(_SC_PAGESIZE);
}

MACHINE_API int
machinarium_init(void)
{
	mm_machinemgr_init(&machinarium.machine_mgr);
	mm_msgcache_init(&machinarium.msg_cache);
	size_t page_size;
	page_size = machinarium_page_size();
	mm_coroutine_cache_init(&machinarium.coroutine_cache,
	                        page_size * 3,
	                        page_size, 100);
	mm_tls_init();
	mm_taskmgr_init(&machinarium.task_mgr);
	mm_taskmgr_start(&machinarium.task_mgr, 3);
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
