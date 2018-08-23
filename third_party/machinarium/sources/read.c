
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

void
mm_readahead_cb(mm_fd_t *handle)
{
	mm_io_t *io = handle->on_read_arg;
	mm_call_t *call = &io->call;
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
			io->connected = 0;

			if (mm_call_is(call, MM_CALL_READ)) {
				call->status = errno;
				mm_scheduler_wakeup(&mm_self->scheduler, call->coroutine);
			}
			return;
		}
		io->readahead_pos += rc;
		left = io->readahead_size - io->readahead_pos;
		assert(left >= 0);
		if (rc == 0) {
			/* eof */
			mm_readahead_stop(io);
			io->connected = 0;
			io->read_eof = 1;
			io->readahead_status = 0;
			if (mm_call_is(call, MM_CALL_READ))
				call->status = 0;
			break;
		}
		break;
	}
	io->readahead_status = 0;

	if (mm_call_is(call, MM_CALL_READ)) {
		call->status = 0;
		int ra_left = io->readahead_pos - io->readahead_pos_read;
		if (io->read_eof || ra_left >= io->read_size)
			mm_scheduler_wakeup(&mm_self->scheduler, call->coroutine);
	}
}

int mm_readahead_start(mm_io_t *io, mm_fd_callback_t callback, void *arg)
{
	mm_machine_t *machine = mm_self;

	int rc;
	rc = mm_buf_ensure(&io->readahead_buf, io->readahead_size);
	if (rc == -1) {
		mm_errno_set(ENOMEM);
		return -1;
	}
	rc = mm_loop_read(&machine->loop, &io->handle, callback, arg);
	if (rc == -1) {
		mm_errno_set(errno);
		return -1;
	}
	return 0;
}

int mm_readahead_stop(mm_io_t *io)
{
	mm_machine_t *machine = mm_self;
	int rc;
	rc = mm_loop_read_stop(&machine->loop, &io->handle);
	if (rc == -1) {
		mm_errno_set(errno);
		return -1;
	}
	return 0;
}

static int
mm_readahead_read(mm_io_t *io, uint32_t time_ms)
{
	assert(io->read_size <= io->readahead_size);

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
		mm_errno_set(io->readahead_status);
		return -1;
	}
	if (io->read_eof || !io->connected) {
		mm_errno_set(ECONNRESET);
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
	mm_buf_reset(&io->readahead_buf);

	/* maybe allocate readahead buffer and-or start io */
	int rc;
	rc = mm_readahead_start(io, mm_readahead_cb, io);
	if (rc == -1)
		return -1;

	/* wait for completion */
	mm_call(&io->call, MM_CALL_READ, time_ms);

	rc = io->call.status;
	if (rc == 0)
		rc = io->readahead_status;
	if (rc != 0) {
		mm_errno_set(rc);
		return -1;
	}
	ra_left = io->readahead_pos - io->readahead_pos_read;
	if (ra_left < io->read_size) {
		mm_errno_set(ECONNRESET);
		return -1;
	}

	memcpy(io->read_buf + copy_pos,
	       io->readahead_buf.start + io->readahead_pos_read,
	       io->read_size);
	io->readahead_pos_read += io->read_size;
	return 0;
}

static inline int
mm_read_chunk(mm_io_t *io, char *buf, int size, uint32_t time_ms)
{
	mm_errno_set(0);
	if (mm_call_is_active(&io->call)) {
		mm_errno_set(EINPROGRESS);
		return -1;
	}
	if (! io->attached) {
		mm_errno_set(ENOTCONN);
		return -1;
	}
	io->read_buf  = buf;
	io->read_size = size;
	io->read_pos  = 0;
	return mm_readahead_read(io, time_ms);
}

int mm_read(mm_io_t *io, char *buf, int size, uint32_t time_ms)
{
	/* split read buffer into readahead-sized chunks */
	int total = 0;
	while (total < size)
	{
		int to_read = size - total;
		if (to_read > io->readahead_size)
			to_read = io->readahead_size;
		int rc;
		rc = mm_read_chunk(io, buf + total, to_read, time_ms);
		if (rc == -1)
			return -1;
		total += to_read;
	}
	return 0;
}

MACHINE_API int
machine_read_to(machine_io_t *obj, machine_msg_t *msg, int size, uint32_t time_ms)
{
	mm_io_t *io = mm_cast(mm_io_t*, obj);

	int position = machine_msg_get_size(msg);
	int rc;
	rc = machine_msg_write(msg, NULL, size);
	if (rc == -1)
		return -1;
	char *buf;
	buf = machine_msg_get_data(msg) + position;
	if (mm_tlsio_is_active(&io->tls))
		rc = mm_tlsio_read(&io->tls, buf, size, time_ms);
	else
		rc = mm_read(io, buf, size, time_ms);
	return rc;
}

MACHINE_API machine_msg_t*
machine_read(machine_io_t *obj, int size, uint32_t time_ms)
{
	machine_msg_t *result;
	result = machine_msg_create(0);
	if (result == NULL)
		return NULL;
	int rc;
	rc = machine_read_to(obj, result, size, time_ms);
	if (rc == -1) {
		machine_msg_free(result);
		result = NULL;
	}
	return result;
}

MACHINE_API int
machine_set_readahead(machine_io_t *obj, int size)
{
	mm_io_t *io = mm_cast(mm_io_t*, obj);
	mm_errno_set(0);
	int rc;
	rc = mm_buf_ensure(&io->readahead_buf, size);
	if (rc == -1) {
		mm_errno_set(ENOMEM);
		return -1;
	}
	io->readahead_size = size;
	return 0;
}

MACHINE_API int
machine_read_pending(machine_io_t *obj)
{
	mm_io_t *io = mm_cast(mm_io_t*, obj);

	mm_errno_set(0);
	if (mm_call_is_active(&io->call)) {
		mm_errno_set(EINPROGRESS);
		return -1;
	}
	if (! io->attached) {
		mm_errno_set(ENOTCONN);
		return -1;
	}

	/* check if io has any pending read data */
	int ra_left = io->readahead_pos - io->readahead_pos_read;
	if (ra_left > 0)
		return 1;

	/* check if there are any data buffered inside SSL context */
	if (mm_tlsio_is_active(&io->tls) && mm_tlsio_read_pending(&io->tls))
		return 1;

	/* check if io has error status set */
	if (io->readahead_status != 0)
		return 1;

	/* check if io reached eof */
	if (! io->connected)
		return 1;

	return 0;
}
