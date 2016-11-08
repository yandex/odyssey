
/*
 * flint.
 *
 * Cooperative multitasking engine.
*/

#include <flint_private.h>
#include <flint.h>

FLINT_API int
ft_bind(ftio_t iop, char *addr, int port)
{
	ftio *io = iop;
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
