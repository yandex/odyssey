
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
	mm_call_t *call = &io->call;
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

			if (mm_call_is(call, MM_CALL_WRITE))
				call->status = errno;
			goto wakeup;
		}
		io->write_pos += rc;
		left = io->write_size - io->write_pos;
		assert(left >= 0);
		return;
	}
	if (mm_call_is(call, MM_CALL_WRITE))
		call->status = 0;

wakeup:
	if (mm_call_is(call, MM_CALL_WRITE)) {
		if (call->coroutine)
			mm_scheduler_wakeup(&mm_self->scheduler, call->coroutine);
	}
}

int mm_write(mm_io_t *io, char *buf, int size, uint32_t time_ms)
{
	mm_machine_t *machine = mm_self;
	mm_errno_set(0);

	if (mm_call_is_active(&io->call)) {
		mm_errno_set(EINPROGRESS);
		return -1;
	}
	if (! io->connected) {
		mm_errno_set(ENOTCONN);
		return -1;
	}
	if (! io->attached) {
		mm_errno_set(ENOTCONN);
		return -1;
	}

	io->write_buf  = buf;
	io->write_size = size;
	io->write_pos  = 0;

	io->handle.on_write = mm_write_cb;
	io->handle.on_write_arg = io;
	mm_call_fast(&io->call, MM_CALL_WRITE,
	             (void(*)(void*))mm_write_cb,
	             &io->handle);
	if (io->call.status != 0) {
		mm_errno_set(io->call.status);
		return -1;
	}
	if (io->write_pos == io->write_size)
		return 0;

	/* subscribe for write event */
	int rc;
	rc = mm_loop_write(&machine->loop, &io->handle, mm_write_cb, io);
	if (rc == -1) {
		mm_errno_set(errno);
		return -1;
	}

	/* wait for completion */
	mm_call(&io->call, MM_CALL_WRITE, time_ms);

	rc = mm_loop_write_stop(&machine->loop, &io->handle);
	if (rc == -1) {
		mm_errno_set(errno);
		return -1;
	}

	rc = io->call.status;
	if (rc != 0) {
		mm_errno_set(rc);
		return -1;
	}
	return 0;
}

static inline int
mm_write_eventfd(mm_io_t *io, char *buf, int size)
{
	if (size != sizeof(uint64_t)) {
		mm_errno_set(EINVAL);
		return -1;
	}
	int rc;
	rc = mm_socket_write(io->fd, buf, size);
	if (rc == -1)
		return -1;
	assert(rc == sizeof(uint64_t));
	return 0;
}

MACHINE_API int
machine_write(machine_io_t *obj, char *buf, int size, uint32_t time_ms)
{
	mm_io_t *io = mm_cast(mm_io_t*, obj);
	if (io->is_eventfd)
		return mm_write_eventfd(io, buf, size);
	if (mm_tlsio_is_active(&io->tls))
		return mm_tlsio_write(&io->tls, buf, size, time_ms);
	return mm_write(io, buf, size, time_ms);
}
