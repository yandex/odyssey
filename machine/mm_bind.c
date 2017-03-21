
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machine_private.h>
#include <machine.h>

MACHINE_API int
machine_bind(machine_io_t obj, struct sockaddr *sa)
{
	mm_io_t *io = obj;
	int rc;
	rc = uv_tcp_bind(&io->handle, sa, 0);
	if (rc < 0)
		return rc;
	return 0;
}
