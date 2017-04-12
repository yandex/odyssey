
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
	for (;;)
	{
		int size;
		size = mm_socket_read(handle->fd,
		                      io->read_ahead.pos,
		                      mm_buf_unused(&io->read_ahead));

		/* read error */
		if (size == -1) {
			if (errno == EAGAIN ||
				errno == EWOULDBLOCK)
				return 0;
			if (errno == EINTR)
				continue;
			io->read_status = errno;
			break;
		}

		/* connection closed */
		if (size == 0) {
			io->connected = 0;
			io->read_status = 0;
			mm_loop_read(&machine->loop, &io->handle, NULL, NULL, 0);
			break;
		}

		/* handle incoming data */
		mm_buf_advance(&io->read_ahead, size);
		assert(mm_buf_used(&io->read_ahead) <= io->read_ahead_size);

		/* readahead buffer now has atleast as minimum
		 * size requested by read operation */
		if (mm_buf_used(&io->read_ahead) >= io->read_size) {
			io->read_ahead_pos = io->read_size;
			break;
		}

		/* stop reader when we reach readahead limit */
		if (mm_buf_used(&io->read_ahead) == io->read_ahead_size) {
			mm_loop_read(&machine->loop, &io->handle, NULL, NULL, 0);
			break;
		}
	}

	if (io->read_fiber)
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
	if (size > io->read_ahead_size) {
		mm_io_set_errno(io, EINVAL);
		return -1;
	}
	io->read_status   = 0;
	io->read_timedout = 0;
	io->read_size     = 0;
	io->read_fiber    = NULL;

	/* allocate readahead buffer */
	int rc;
	if (! mm_buf_size(&io->read_ahead)) {
		rc = mm_buf_ensure(&io->read_ahead, io->read_ahead_size);
		if (rc == -1) {
			mm_io_set_errno(io, ENOMEM);
			return -1;
		}
	}

	/* use readhead */
	int ra_left = mm_buf_used(&io->read_ahead) - io->read_ahead_pos;
	if (ra_left >= size) {
		io->read_ahead_pos_data = io->read_ahead_pos;
		io->read_ahead_pos += size;
		if (buf) {
			memcpy(buf, io->read_ahead.start + io->read_ahead_pos_data, size);
		}
		return 0;
	}

	if (! io->connected) {
		mm_io_set_errno(io, ENOTCONN);
		return -1;
	}

	/* readahead has insufficient data */
	if (ra_left > 0) {
		memmove(io->read_ahead.start,
		        io->read_ahead.start + io->read_ahead_pos, ra_left);
		mm_buf_reset(&io->read_ahead);
		mm_buf_advance(&io->read_ahead, ra_left);
	} else {
		mm_buf_reset(&io->read_ahead);
	}
	io->read_size           = size;
	io->read_ahead_pos_data = 0;
	io->read_ahead_pos      = 0;

	/* maybe subscribe for read event */
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

	if (mm_buf_used(&io->read_ahead) >= io->read_size) {
		if (buf) {
			memcpy(buf, io->read_ahead.start + io->read_ahead_pos_data, size);
		}
		return 0;
	}
	rc = io->read_status;
	assert(rc != 0);
	mm_io_set_errno(io, rc);
	return -1;
}

MACHINE_API int
machine_read(machine_io_t obj, char *buf, int size, uint64_t time_ms)
{
	mm_io_t *io = obj;
	/*
	if (mm_tls_is_active(&io->tls))
		return mm_tlsio_read(&io->tls, buf, size);
		*/
	return mm_read(io, buf, size, time_ms);
}

MACHINE_API int
machine_read_timedout(machine_io_t obj)
{
	mm_io_t *io = obj;
	return io->read_timedout;
}

MACHINE_API char*
machine_read_buf(machine_io_t obj)
{
	mm_io_t *io = obj;
	return io->read_ahead.start + io->read_ahead_pos_data;
}
