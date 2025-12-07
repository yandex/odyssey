/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>

#include <machinarium/machinarium.h>
#include <machinarium/io.h>
#include <machinarium/socket.h>
#include <machinarium/machine.h>

MACHINE_API int machine_eventfd(machine_io_t *obj)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	mm_errno_set(0);
	if (io->connected) {
		mm_errno_set(EINPROGRESS);
		return -1;
	}
	int rc;
	rc = mm_socket_eventfd(0);
	if (rc == -1) {
		return -1;
	}
	io->fd = rc;
	io->handle.fd = io->fd;
	io->is_eventfd = 1;
	io->connected = 1;
	return 0;
}
