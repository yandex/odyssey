
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
	mm_queuerdpool_init(&machinarium.queuerd_pool);
	mm_tls_init();
	return 0;
}

MACHINE_API void
machinarium_free(void)
{
	mm_machinemgr_free(&machinarium.machine_mgr);
	mm_queuerdpool_free(&machinarium.queuerd_pool);
	mm_msgpool_free(&machinarium.msg_pool);
	mm_tls_free();
}
