
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

static void
mm_write_timer_cb(mm_timer_t *handle)
{
	mm_io_t *io = handle->arg;
	io->write_status = ETIMEDOUT;
	io->write_timedout = 1;
	mm_scheduler_wakeup(io->write_fiber);
}

static void
mm_write_cancel_cb(void *obj, void *arg)
{
	mm_io_t *io = arg;
	(void)obj;
	io->write_status = ECANCELED;
	mm_scheduler_wakeup(io->write_fiber);
}

static int
mm_write_cb(mm_fd_t *handle)
{
	mm_io_t *io = handle->on_write_arg;
	int left = io->write_size - io->write_pos;
	int rc;
	while (left > 0)
	{
		rc = mm_socket_write(io->fd, io->write_buf + io->write_pos, left);
		if (rc == -1) {
			if (errno == EAGAIN ||
			    errno == EWOULDBLOCK)
				return 0;
			if (errno == EINTR)
				continue;
			io->connect_status = errno;
			mm_scheduler_wakeup(io->connect_fiber);
			return 0;
		}
		io->write_pos += rc;
		left = io->write_size - io->write_pos;
		assert(left >= 0);
	}
	io->connect_status = 0;
	mm_scheduler_wakeup(io->write_fiber);
	return 0;
}

int
mm_write(mm_io_t *io, char *buf, int size, uint64_t time_ms)
{
	mm_t *machine = machine = io->machine;
	mm_fiber_t *current = mm_scheduler_current(&io->machine->scheduler);
	mm_io_set_errno(io, 0);

	if (mm_fiber_is_cancelled(current)) {
		mm_io_set_errno(io, ECANCELED);
		return -1;
	}
	if (io->write_fiber) {
		mm_io_set_errno(io, EINPROGRESS);
		return -1;
	}
	if (! io->connected) {
		mm_io_set_errno(io, ENOTCONN);
		return -1;
	}

	io->write_status   = 0;
	io->write_timedout = 0;
	io->write_fiber    = NULL;
	io->write_buf      = buf;
	io->write_size     = size;
	io->write_pos      = 0;

	/* subscribe for write event */
	int rc;
	rc = mm_loop_write(&machine->loop, &io->handle, mm_write_cb, io, 1);
	if (rc == -1) {
		mm_io_set_errno(io, errno);
		return -1;
	}

	/* wait for timedout, cancel or execution status */
	mm_timer_start(&machine->loop.clock,
	               &io->write_timer,
	               mm_write_timer_cb, io, time_ms);
	mm_call_begin(&current->call, mm_write_cancel_cb, io);
	io->write_fiber = current;
	mm_scheduler_yield(&machine->scheduler);
	io->write_fiber = NULL;
	mm_call_end(&current->call);
	mm_timer_stop(&io->write_timer);

	rc = mm_loop_write(&machine->loop, &io->handle, NULL, NULL, 0);
	if (rc == -1) {
		mm_io_set_errno(io, errno);
		return -1;
	}
	rc = io->write_status;
	if (rc != 0) {
		mm_io_set_errno(io, rc);
		return -1;
	}
	return 0;
}

MACHINE_API int
machine_write(machine_io_t obj, char *buf, int size, uint64_t time_ms)
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
	return io->write_timedout;
}
