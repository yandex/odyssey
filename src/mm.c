
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

mm_t machinarium;

MACHINE_API int
machinarium_init(void)
{
	mm_machinemgr_init(&machinarium.machine_mgr);
	mm_msgpool_init(&machinarium.msg_pool);
	mm_tls_init();
	mm_taskmgr_init(&machinarium.task_mgr);
	mm_taskmgr_start(&machinarium.task_mgr, 3);
	return 0;
}

MACHINE_API void
machinarium_free(void)
{
	mm_taskmgr_stop(&machinarium.task_mgr);
	mm_machinemgr_free(&machinarium.machine_mgr);
	mm_msgpool_free(&machinarium.msg_pool);
	mm_tls_free();
}
