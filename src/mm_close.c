
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

MACHINE_API int
machine_close(machine_io_t obj)
{
	mm_io_t *io = obj;
	mm_machine_t *machine = machine = io->machine;
	if (io->fd == -1) {
		mm_io_set_errno(io, EBADF);
		return -1;
	}
	int rc;
	rc = mm_loop_delete(&machine->loop, &io->handle);
	if (rc == -1)
		mm_io_set_errno(io, errno);
	rc = close(io->fd);
	if (rc == -1)
		mm_io_set_errno(io, errno);
	io->connected = 0;
	io->fd = -1;
	io->handle.fd = -1;
	io->handle.on_read = NULL;
	io->handle.on_write = NULL;
	return 0;
}
