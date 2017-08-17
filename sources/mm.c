
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

static int machinarium_initialized = 0;
mm_t       machinarium;

MACHINE_API int
machinarium_init(void)
{
	mm_machinemgr_init(&machinarium.machine_mgr);
	mm_msgcache_init(&machinarium.msg_cache);
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
	mm_tls_free();
}
