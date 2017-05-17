
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

MACHINE_API int
machinarium_init(void)
{
	mm_tls_init();
	return 0;
}

MACHINE_API void
machinarium_free(void)
{ }
