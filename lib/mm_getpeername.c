
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

MM_API int
mm_getpeername(mm_io_t iop, struct sockaddr *sa, int *salen)
{
	mmio *io = iop;
	int rc = uv_tcp_getpeername(&io->handle, sa, salen);
	if (rc < 0)
		return rc;
	return 0;
}
