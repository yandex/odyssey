
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

MM_API int
mm_bind(mm_io_t iop, struct sockaddr *sa)
{
	mmio *io = iop;
	int rc;
	rc = uv_tcp_bind(&io->handle, sa, 0);
	if (rc < 0)
		return rc;
	return 0;
}
