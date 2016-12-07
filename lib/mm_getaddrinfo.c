
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

MM_API int
mm_getaddrinfo(mm_io_t iop, char *addr, char *service,
               struct addrinfo *hints,
               struct addrinfo **res)
{
	(void)iop;
	(void)addr;
	(void)service;
	(void)hints;
	(void)res;
	return 0;
}
