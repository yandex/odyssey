
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

MM_API int
mm_bind(mm_io_t iop, char *addr, int port)
{
	mmio *io = iop;
	struct sockaddr_in saddr;
	int rc;
	rc = uv_ip4_addr(addr, port, &saddr);
	if (rc < 0)
		return rc;
	rc = uv_tcp_bind(&io->handle, (struct sockaddr*)&saddr, 0);
	if (rc < 0)
		return rc;
	return 0;
}
