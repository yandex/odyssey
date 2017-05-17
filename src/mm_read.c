
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

static void
mm_read_cb(mm_fd_t *handle)
{
	mm_io_t *io = handle->on_read_arg;
	mm_machine_t *machine = machine = io->machine;
	mm_call_t *call = &io->read;
	if (mm_call_is_aborted(call))
		return;

	int left = io->read_size - io->read_pos;
	int rc;
	while (left > 0)
	{
		rc = mm_socket_read(io->fd, io->read_buf + io->read_pos, left);
		if (rc == -1) {
			if (errno == EAGAIN ||
			    errno == EWOULDBLOCK)
				return;
			if (errno == EINTR)
				continue;
			call->status = errno;
			goto wakeup;
		}
		io->read_pos += rc;
		left = io->read_size - io->read_pos;
		assert(left >= 0);
		if (rc == 0) {
			/* eof */
			io->read_eof = 1;
			call->status = 0;
			goto wakeup;
		}
		break;
	}
	call->status = 0;

wakeup:
	if (call->fiber)
		mm_scheduler_wakeup(call->fiber);
}

static int
mm_read_default(mm_io_t *io, uint64_t time_ms)
{
	mm_machine_t *machine = machine = io->machine;
	io->read_eof = 0;
	io->handle.on_read = mm_read_cb;
	io->handle.on_read_arg = io;
	mm_call_fast(&io->write, &machine->scheduler,
	             (void(*)(void*))mm_read_cb,
	             &io->handle);
	if (io->read.status != 0) {
		mm_io_set_errno(io, io->read.status);
		return -1;
	}
	if (io->read_pos == io->read_size)
		return 0;

	/* subscribe for read event */
	int rc;
	rc = mm_loop_read(&machine->loop, &io->handle, mm_read_cb, io, 1);
	if (rc == -1) {
		mm_io_set_errno(io, errno);
		return -1;
	}

	/* wait for completion */
	mm_call(&io->read,
	        &machine->scheduler,
	        &machine->loop.clock, time_ms);

	rc = mm_loop_read(&machine->loop, &io->handle, NULL, NULL, 0);
	if (rc == -1) {
		mm_io_set_errno(io, errno);
		return -1;
	}

	rc = io->read.status;
	if (rc == 0) {
		if (io->read_pos != io->read_size) {
			mm_io_set_errno(io, ECONNRESET);
			return -1;
		}
		return 0;
	}
	mm_io_set_errno(io, rc);
	return -1;
}

static int
mm_readahead_read(mm_io_t *io, uint64_t time_ms);

int mm_readahead_stop(mm_io_t *io);

static void
mm_readahead_cb(mm_fd_t *handle)
{
	mm_io_t *io = handle->on_read_arg;
	mm_machine_t *machine = machine = io->machine;
	mm_call_t *call = &io->read;
	if (mm_call_is_aborted(call))
		return;

	int left = io->readahead_size - io->readahead_pos;
	int rc;
	while (left > 0)
	{
		rc = mm_socket_read(io->fd, io->readahead_buf.start + io->readahead_pos, left);
		if (rc == -1) {
			if (errno == EAGAIN ||
			    errno == EWOULDBLOCK)
				break;
			if (errno == EINTR)
				continue;
			io->readahead_status = errno;

			if (mm_call_is_active(call)) {
				call->status = errno;
				if (call->fiber)
					mm_scheduler_wakeup(call->fiber);
			}
			return;
		}
		io->readahead_pos += rc;
		left = io->readahead_size - io->readahead_pos;
		assert(left >= 0);
		if (rc == 0) {
			/* eof */
			mm_readahead_stop(io);
			io->read_eof = 1;
			io->readahead_status = 0;
			if (mm_call_is_active(call))
				call->status = 0;
			break;
		}
		break;
	}
	io->readahead_status = 0;

	if (mm_call_is_active(call)) {
		call->status = 0;
		int ra_left = io->readahead_pos - io->readahead_pos_read;
		if (io->read_eof || ra_left >= io->read_size)
			mm_scheduler_wakeup(call->fiber);
	}
}

int
mm_readahead_start(mm_io_t *io)
{
	mm_machine_t *machine = machine = io->machine;
	int rc;
	rc = mm_loop_read(&machine->loop, &io->handle, mm_readahead_cb, io, 1);
	if (rc == -1) {
		mm_io_set_errno(io, errno);
		return -1;
	}
	return 0;
}

int
mm_readahead_stop(mm_io_t *io)
{
	mm_machine_t *machine = machine = io->machine;
	int rc;
	rc = mm_loop_read(&machine->loop, &io->handle, NULL, NULL, 0);
	if (rc == -1) {
		mm_io_set_errno(io, errno);
		return -1;
	}
	return 0;
}

static int
mm_readahead_read(mm_io_t *io, uint64_t time_ms)
{
	mm_machine_t *machine = machine = io->machine;

	if (io->read_size > io->readahead_size) {
		mm_io_set_errno(io, EINVAL);
		return -1;
	}

	/* try to use readahead first */
	assert(io->readahead_pos >= io->readahead_pos_read);
	int ra_left = io->readahead_pos - io->readahead_pos_read;
	if (ra_left >= io->read_size) {
		memcpy(io->read_buf, io->readahead_buf.start + io->readahead_pos_read,
		       io->read_size);
		io->readahead_pos_read += io->read_size;
		return 0;
	}
	if (io->readahead_status != 0) {
		mm_io_set_errno(io, io->readahead_status);
		return -1;
	}
	if (io->read_eof) {
		mm_io_set_errno(io, ECONNRESET);
		return -1;
	}

	/* copy readahead chunk */
	int copy_pos = 0;
	if (ra_left > 0) {
		memcpy(io->read_buf,
		       io->readahead_buf.start + io->readahead_pos_read,
		       ra_left);
		io->readahead_pos_read += ra_left;
		io->read_size -= ra_left;
		copy_pos = ra_left;
	}

	/* reset readahead position */
	assert(io->readahead_pos_read == io->readahead_pos);
	io->readahead_pos = 0;
	io->readahead_pos_read = 0;

	/* wait for completion */
	mm_call(&io->read,
	        &machine->scheduler,
	        &machine->loop.clock, time_ms);

	int rc;
	rc = io->read.status;
	if (rc == 0)
		rc = io->readahead_status;
	if (rc != 0) {
		mm_io_set_errno(io, rc);
		return -1;
	}
	ra_left = io->readahead_pos - io->readahead_pos_read;
	if (ra_left < io->read_size) {
		mm_io_set_errno(io, ECONNRESET);
		return -1;
	}

	memcpy(io->read_buf + copy_pos,
		   io->readahead_buf.start + io->readahead_pos_read,
		   io->read_size);
	io->readahead_pos_read += io->read_size;
	return 0;
}

int
mm_read(mm_io_t *io, char *buf, int size, uint64_t time_ms)
{
	mm_machine_t *machine = machine = io->machine;
	mm_fiber_t *current = mm_scheduler_current(&io->machine->scheduler);
	mm_io_set_errno(io, 0);
	if (mm_fiber_is_cancelled(current)) {
		mm_io_set_errno(io, ECANCELED);
		return -1;
	}
	if (mm_call_is_active(&io->read)) {
		mm_io_set_errno(io, EINPROGRESS);
		return -1;
	}
	if (! io->connected) {
		mm_io_set_errno(io, ENOTCONN);
		return -1;
	}
	io->read_buf  = buf;
	io->read_size = size;
	io->read_pos  = 0;
	if (io->readahead_size > 0)
		return mm_readahead_read(io, time_ms);
	return mm_read_default(io, time_ms);
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
	return io->read.timedout;
}

MACHINE_API int
machine_set_readahead(machine_io_t obj, int size)
{
	mm_io_t *io = obj;
	mm_fiber_t *current = mm_scheduler_current(&io->machine->scheduler);
	mm_io_set_errno(io, 0);
	if (mm_fiber_is_cancelled(current)) {
		mm_io_set_errno(io, ECANCELED);
		return -1;
	}
	if (mm_call_is_active(&io->read)) {
		mm_io_set_errno(io, EINPROGRESS);
		return -1;
	}
	if (! io->connected) {
		mm_io_set_errno(io, ENOTCONN);
		return -1;
	}
	if (io->readahead_size > 0) {
		mm_io_set_errno(io, EINPROGRESS);
		return -1;
	}
	if (size == 0)
		return 0;
	int rc;
	rc = mm_buf_ensure(&io->readahead_buf, size);
	if (rc == -1) {
		mm_io_set_errno(io, ENOMEM);
		return -1;
	}
	rc = mm_readahead_start(io);
	if (rc == -1)
		return -1;
	io->readahead_size = size;
	return 0;
}
