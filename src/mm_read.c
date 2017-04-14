
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

static void
mm_read_timer_cb(mm_timer_t *handle)
{
	mm_io_t *io = handle->arg;
	io->read_timedout = 1;
	io->read_status = ETIMEDOUT;
	mm_scheduler_wakeup(io->read_fiber);
}

static void
mm_read_cancel_cb(void *obj, void *arg)
{
	(void)obj;
	mm_io_t *io = arg;
	io->read_timedout = 0;
	io->read_status = ECANCELED;
	mm_scheduler_wakeup(io->read_fiber);
}

static int
mm_read_cb(mm_fd_t *handle)
{
	mm_io_t *io = handle->on_read_arg;
	mm_t *machine = machine = io->machine;
	int left = io->read_size - io->read_pos;
	int rc;
	while (left > 0)
	{
		rc = mm_socket_read(io->fd, io->read_buf + io->read_pos, left);
		if (rc == -1) {
			if (errno == EAGAIN ||
			    errno == EWOULDBLOCK)
				return 0;
			if (errno == EINTR)
				continue;
			io->read_status = errno;
			mm_scheduler_wakeup(io->read_fiber);
			return 0;
		}
		io->read_pos += rc;
		left = io->read_size - io->read_pos;
		assert(left >= 0);
		if (rc == 0) {
			/* eof */
			io->read_eof = 1;
			io->read_status = 0;
			mm_scheduler_wakeup(io->read_fiber);
			return 0;
		}
	}
	io->read_status = 0;
	mm_scheduler_wakeup(io->read_fiber);
	return 0;
}

int
mm_read(mm_io_t *io, char *buf, int size, uint64_t time_ms)
{
	mm_t *machine = machine = io->machine;
	mm_fiber_t *current = mm_scheduler_current(&io->machine->scheduler);
	mm_io_set_errno(io, 0);
	if (mm_fiber_is_cancelled(current)) {
		mm_io_set_errno(io, ECANCELED);
		return -1;
	}
	if (io->read_fiber) {
		mm_io_set_errno(io, EINPROGRESS);
		return -1;
	}
	if (! io->connected) {
		mm_io_set_errno(io, ENOTCONN);
		return -1;
	}
	io->read_status   = 0;
	io->read_timedout = 0;
	io->read_eof      = 0;
	io->read_fiber    = NULL;
	io->read_size     = size;
	io->read_pos      = 0;
	io->read_buf      = buf;

	/* subscribe for read event */
	int rc;
	rc = mm_loop_read(&machine->loop, &io->handle, mm_read_cb, io, 1);
	if (rc == -1) {
		mm_io_set_errno(io, errno);
		return -1;
	}

	/* wait for timedout, cancel or execution status */
	mm_timer_start(&machine->loop.clock,
	               &io->read_timer,
	               mm_read_timer_cb, io, time_ms);
	mm_call_begin(&current->call, mm_read_cancel_cb, io);
	io->read_fiber = current;
	mm_scheduler_yield(&machine->scheduler);
	io->read_fiber = NULL;
	mm_call_end(&current->call);
	mm_timer_stop(&io->read_timer);

	rc = mm_loop_read(&machine->loop, &io->handle, NULL, NULL, 0);
	if (rc == -1) {
		mm_io_set_errno(io, errno);
		return -1;
	}

	rc = io->read_status;
	if (rc == 0) {
		if (io->read_pos != size) {
			mm_io_set_errno(io, ECONNRESET);
			return -1;
		}
		return 0;
	}
	mm_io_set_errno(io, rc);
	return -1;
}

MACHINE_API int
machine_read(machine_io_t obj, char *buf, int size, uint64_t time_ms)
{
	mm_io_t *io = obj;
	if (mm_tls_is_active(&io->tls))
		return mm_tlsio_read(&io->tls, buf, size);
	return mm_read(io, buf, size, time_ms);
}

MACHINE_API int
machine_read_timedout(machine_io_t obj)
{
	mm_io_t *io = obj;
	return io->read_timedout;
}
