
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

MACHINE_API int
machine_bind(machine_io_t obj, struct sockaddr *sa)
{
	mm_io_t *io = obj;
	mm_machine_t *machine = mm_self;
	mm_io_set_errno(io, 0);
	if (io->connected) {
		mm_io_set_errno(io, EINPROGRESS);
		return -1;
	}
	int rc;
	rc = mm_io_socket(io, sa);
	if (rc == -1)
		goto error;
	rc = mm_socket_set_reuseaddr(io->fd, 1);
	if (rc == -1) {
		mm_io_set_errno(io, errno);
		goto error;
	}
	rc = mm_socket_bind(io->fd, sa);
	if (rc == -1) {
		mm_io_set_errno(io, errno);
		goto error;
	}
	rc = mm_loop_add(&machine->loop, &io->handle, 0);
	if (rc == -1) {
		mm_io_set_errno(io, errno);
		goto error;
	}
	return 0;
error:
	if (io->fd != -1) {
		close(io->fd);
		io->fd = -1;
	}
	io->handle.fd = -1;
	return -1;
}
