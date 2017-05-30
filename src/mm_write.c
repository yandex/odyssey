
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

static void
mm_write_cb(mm_fd_t *handle)
{
	mm_io_t *io = handle->on_write_arg;
	mm_call_t *call = &io->write;
	if (mm_call_is_aborted(call))
		return;
	int left = io->write_size - io->write_pos;
	int rc;
	while (left > 0)
	{
		rc = mm_socket_write(io->fd, io->write_buf + io->write_pos, left);
		if (rc == -1) {
			if (errno == EAGAIN ||
			    errno == EWOULDBLOCK)
				return;
			if (errno == EINTR)
				continue;
			call->status = errno;
			goto wakeup;
		}
		io->write_pos += rc;
		left = io->write_size - io->write_pos;
		assert(left >= 0);
		return;
	}
	call->status = 0;
wakeup:
	if (call->coroutine)
		mm_scheduler_wakeup(&mm_self->scheduler, call->coroutine);
}

int mm_write(mm_io_t *io, char *buf, int size, uint32_t time_ms)
{
	mm_machine_t *machine = mm_self;
	mm_coroutine_t *current;
	current = mm_scheduler_current(&machine->scheduler);
	mm_io_set_errno(io, 0);

	if (mm_coroutine_is_cancelled(current)) {
		mm_io_set_errno(io, ECANCELED);
		return -1;
	}
	if (mm_call_is_active(&io->write)) {
		mm_io_set_errno(io, EINPROGRESS);
		return -1;
	}
	if (! io->connected) {
		mm_io_set_errno(io, ENOTCONN);
		return -1;
	}
	if (! io->attached) {
		mm_io_set_errno(io, ENOTCONN);
		return -1;
	}

	io->write_buf  = buf;
	io->write_size = size;
	io->write_pos  = 0;

	io->handle.on_write = mm_write_cb;
	io->handle.on_write_arg = io;
	mm_call_fast(&io->write, (void(*)(void*))mm_write_cb,
	             &io->handle);
	if (io->write.status != 0) {
		mm_io_set_errno(io, io->write.status);
		return -1;
	}
	if (io->write_pos == io->write_size)
		return 0;

	/* subscribe for write event */
	int rc;
	rc = mm_loop_write(&machine->loop, &io->handle, mm_write_cb, io);
	if (rc == -1) {
		mm_io_set_errno(io, errno);
		return -1;
	}

	/* wait for completion */
	mm_call(&io->write, time_ms);

	rc = mm_loop_write_stop(&machine->loop, &io->handle);
	if (rc == -1) {
		mm_io_set_errno(io, errno);
		return -1;
	}

	rc = io->write.status;
	if (rc != 0) {
		mm_io_set_errno(io, rc);
		return -1;
	}
	return 0;
}

MACHINE_API int
machine_write(machine_io_t obj, char *buf, int size, uint32_t time_ms)
{
	mm_io_t *io = obj;
	if (mm_tls_is_active(&io->tls))
		return mm_tlsio_write(&io->tls, buf, size);
	return mm_write(io, buf, size, time_ms);
}

MACHINE_API int
machine_write_timedout(machine_io_t obj)
{
	mm_io_t *io = obj;
	return io->write.timedout;
}
