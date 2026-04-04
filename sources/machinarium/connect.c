
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>

#include <machinarium/machinarium.h>
#include <machinarium/fd.h>
#include <machinarium/io.h>
#include <machinarium/machine.h>
#include <machinarium/socket.h>

static int mm_connect(mm_io_t *io, struct sockaddr *sa, uint32_t time_ms)
{
	mm_machine_t *machine = mm_self;
	mm_errno_set(0);

	if (mm_call_is_active(&io->call)) {
		mm_errno_set(EINPROGRESS);
		return -1;
	}
	if (io->connected) {
		mm_errno_set(EINPROGRESS);
		return -1;
	}

	/* create socket */
	int rc;
	rc = mm_io_socket(io, sa);
	if (rc == -1) {
		goto error;
	}

	/* start connection */
	rc = mm_socket_connect(io->fd, sa);
	if (rc == 0) {
		rc = mm_io_attach(io);
		if (rc == -1) {
			goto error;
		}
		goto done;
	}

	assert(rc == -1);
	if (errno != EINPROGRESS) {
		mm_errno_set(errno);
		goto error;
	}

	/* add socket to event loop */
	rc = mm_io_attach(io);
	if (rc == -1) {
		goto error;
	}

	/*
	 * wait for completion
	 */
	rc = mm_io_wait(io, time_ms);
	if (rc == -1) {
		mm_io_detach((mm_io_t *)io);
		goto error;
	}

	rc = mm_socket_error(io->handle.fd);
	if (rc != 0) {
		mm_loop_delete(&machine->loop, &io->handle);
		mm_errno_set(rc);
		goto error;
	}

done:
	assert(!io->call.timedout);
	io->connected = 1;
	return 0;

error:
	if (io->fd != -1) {
		close(io->fd);
		io->fd = -1;
	}
	io->handle.fd = -1;
	io->attached = 0;
	return -1;
}

MACHINE_API int mm_io_connect(mm_io_t *obj, struct sockaddr *sa,
			      uint32_t time_ms)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	int rc = mm_connect(io, sa, time_ms);
	if (rc == -1) {
		return -1;
	}
	return 0;
}

MACHINE_API int mm_io_connected(mm_io_t *obj)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	return io->connected;
}
