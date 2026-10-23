
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium/machinarium.h>
#include <machinarium/mm.h>
#include <machinarium/memory.h>
#include <machinarium/tls.h>

static int machinarium_stack_size = 0;
static int machinarium_system_stack_size = 0;
static int machinarium_pool_size = 0;
static int machinarium_coroutine_cache_size = 0;
static int machinarium_msg_cache_gc_size = 0;
static int machinarium_msg_cache_gc_count = 0;
static int machinarium_initialized = 0;
static int machinarium_dns_ttl_ms = -1;
static int machinarium_file_pool_size = -1;
mm_t machinarium;

static inline size_t machinarium_page_size(void)
{
	return sysconf(_SC_PAGESIZE);
}

MACHINE_API void machinarium_set_file_pool_size(int size)
{
	machinarium_file_pool_size = size;
}

MACHINE_API void machinarium_set_stack_size(int size)
{
	machinarium_stack_size = size;
}

MACHINE_API void machinarium_set_system_stack_size(int size)
{
	machinarium_system_stack_size = size;
}

MACHINE_API void machinarium_set_pool_size(int size)
{
	machinarium_pool_size = size;
}

MACHINE_API void machinarium_set_coroutine_cache_size(int size)
{
	machinarium_coroutine_cache_size = size;
}

MACHINE_API void machinarium_set_msg_cache_gc_size(int size)
{
	machinarium_msg_cache_gc_size = size;
}

MACHINE_API void machinarium_set_msg_cache_gc_count(int count)
{
	machinarium_msg_cache_gc_count = count;
}

MACHINE_API void machinarium_set_dns_ttl_ms(int ttl_ms)
{
	machinarium_dns_ttl_ms = ttl_ms;
}

MACHINE_API int machinarium_init(void)
{
	if (machinarium_initialized) {
		return -1;
	}

	if (machinarium_stack_size <= 0) {
		machinarium_stack_size = 4;
	}

	if (machinarium_system_stack_size <= 0) {
		machinarium_system_stack_size = 32;
	}

	if (machinarium_pool_size == 0) {
		machinarium_pool_size = 1;
	}

	if (machinarium_dns_ttl_ms < 0) {
		machinarium_dns_ttl_ms = 30 * 1000;
	}

	if (machinarium_file_pool_size < 0) {
		machinarium_file_pool_size = 2;
	}

	machinarium.config.page_size = machinarium_page_size();
	machinarium.config.stack_size = machinarium_stack_size;
	machinarium.config.system_stack_size = machinarium_system_stack_size;
	machinarium.config.pool_size = machinarium_pool_size;
	machinarium.config.coroutine_cache_size =
		machinarium_coroutine_cache_size;
	machinarium.config.msg_cache_gc_size = machinarium_msg_cache_gc_size;
	machinarium.config.msg_cache_gc_count = machinarium_msg_cache_gc_count;
	machinarium.config.dns_ttl_ms = machinarium_dns_ttl_ms;
	machinarium.config.file_pool_size = machinarium_file_pool_size;

	mm_machinemgr_init(&machinarium.machine_mgr);
	mm_tls_engine_init();
	mm_taskmgr_init(&machinarium.task_mgr);
	mm_taskmgr_start(&machinarium.task_mgr, machinarium.config.pool_size,
			 machinarium.config.dns_ttl_ms);

	/*
	 * The file pool workers are machines, so the pool can be
	 * started only when the machine manager is ready.
	 */
	machinarium.file_pool = NULL;
	if (machinarium_file_pool_size > 0) {
		mm_thread_pool_t *pool = mm_malloc(sizeof(mm_thread_pool_t));
		if (pool == NULL) {
			goto error;
		}
		int rc = mm_thread_pool_init(
			pool, "file", (size_t)machinarium_file_pool_size, 1024);
		if (rc != 0) {
			mm_free(pool);
			goto error;
		}
		machinarium.file_pool = pool;
	}

	machinarium_initialized = 1;
	return 0;

error:
	mm_taskmgr_stop(&machinarium.task_mgr);
	mm_machinemgr_free(&machinarium.machine_mgr);
	mm_tls_engine_free();
	return -1;
}

MACHINE_API void machinarium_free(void)
{
	if (!machinarium_initialized) {
		return;
	}
	/* the file pool workers are machines, stop them first */
	if (machinarium.file_pool != NULL) {
		mm_thread_pool_destroy(machinarium.file_pool);
		mm_free(machinarium.file_pool);
		machinarium.file_pool = NULL;
	}
	mm_taskmgr_stop(&machinarium.task_mgr);
	mm_machinemgr_free(&machinarium.machine_mgr);
	mm_tls_engine_free();
	machinarium_initialized = 0;
}
