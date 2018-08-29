
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

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

static void
mm_write_cb(mm_fd_t *handle)
{
	mm_io_t *io = handle->on_write_arg;
	mm_call_t *call = &io->call;
	if (mm_call_is_aborted(call))
		return;

	int iov_to_write = io->write_queue_count;
	if (iov_to_write > IOV_MAX)
		iov_to_write = IOV_MAX;

	struct iovec *iov;
	iov = (struct iovec*)io->write_iov.start + io->write_iov_pos;
	int rc;
	rc = mm_socket_writev(io->fd, iov, iov_to_write);
	if (rc == -1) {
		if (errno == EAGAIN || errno == EINTR)
			return;

		io->write_status = errno;
		if (mm_call_is(call, MM_CALL_FLUSH))
			call->status = io->write_status;

		goto wakeup;
	}
	int written = rc;

	while (io->write_queue_count > 0)
	{
		if (iov->iov_len > (size_t)written) {
			iov->iov_base = (char*)iov->iov_base + written;
			iov->iov_len -= written;
			break;
		}

		mm_msg_t *msg;
		msg = mm_container_of(io->write_queue.next, mm_msg_t, link);
		mm_list_unlink(&msg->link);
		machine_msg_free((machine_msg_t*)msg);

		io->write_queue_count--;
		io->write_iov_pos++;

		written -= iov->iov_len;
		iov++;
	}

	if (io->write_queue_count == 0)
	{
		io->write_iov_pos = 0;
		mm_buf_reset(&io->write_iov);

		mm_machine_t *machine = mm_self;
		mm_loop_write_stop(&machine->loop, &io->handle);

		io->write_status = 0;
		goto wakeup;
	}

	return;

wakeup:
	if (mm_call_is(call, MM_CALL_FLUSH)) {
		if (call->coroutine)
			mm_scheduler_wakeup(&mm_self->scheduler, call->coroutine);
	}
}

int
mm_write(mm_io_t *io, machine_msg_t *obj)
{
	mm_machine_t *machine = mm_self;
	mm_msg_t *msg = mm_cast(mm_msg_t*, obj);

	int rc;
	rc = mm_buf_ensure(&io->write_iov, sizeof(struct iovec));
	if (rc == -1) {
		mm_errno_set(ENOMEM);
		return -1;
	}
	struct iovec *iov;
	iov = (struct iovec*)io->write_iov.pos;
	iov->iov_base = msg->data.start;
	iov->iov_len  = mm_buf_used(&msg->data);
	mm_buf_advance(&io->write_iov, sizeof(struct iovec));

	mm_list_append(&io->write_queue, &msg->link);
	io->write_queue_count++;

	rc = mm_loop_write(&machine->loop, &io->handle, mm_write_cb, io);
	if (rc == -1) {
		mm_errno_set(errno);
		return -1;
	}

	return 0;
}

MACHINE_API int
machine_write(machine_io_t *obj, machine_msg_t *msg)
{
	mm_io_t *io = mm_cast(mm_io_t*, obj);
	mm_errno_set(0);

	if (! io->connected) {
		mm_errno_set(ENOTCONN);
		return -1;
	}
	if (! io->attached) {
		mm_errno_set(ENOTCONN);
		return -1;
	}

	char *buf = machine_msg_get_data(msg);
	int   buf_size = machine_msg_get_size(msg);
	int   rc;
	if (io->is_eventfd) {
		rc = mm_write_eventfd(io, buf, buf_size);
		machine_msg_free(msg);
		return rc;
	}
	if (mm_tlsio_is_active(&io->tls)) {
		rc = mm_tlsio_write(&io->tls, buf, buf_size);
		machine_msg_free(msg);
		return rc;
	}

	return mm_write(io, msg);
}

MACHINE_API int
machine_write_batch(machine_io_t *obj, machine_channel_t *obj_channel)
{
	mm_errno_set(0);
	mm_channeltype_t *type;
	type = mm_cast(mm_channeltype_t*, obj_channel);
	if (type->is_shared) {
		mm_errno_set(EINVAL);
		return -1;
	}
	mm_channelfast_t *channel;
	channel = mm_cast(mm_channelfast_t*, obj_channel);
	if (channel->readers_count > 0) {
		mm_errno_set(EINVAL);
		return -1;
	}
	mm_list_t *i, *n;
	mm_list_foreach_safe(&channel->incoming, i, n)
	{
		mm_msg_t *msg;
		msg = mm_container_of(i, mm_msg_t, link);
		mm_list_unlink(&msg->link);
		channel->incoming_count--;
		int rc;
		rc = machine_write(obj, (machine_msg_t*)msg);
		if (rc == -1)
			return -1;
	}
	mm_list_init(&channel->incoming);
	return 0;
}

MACHINE_API int
machine_flush(machine_io_t *obj, uint32_t time_ms)
{
	mm_io_t *io = mm_cast(mm_io_t*, obj);
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

	if (io->write_status != 0) {
		mm_errno_set(io->write_status);
		return -1;
	}

	if (io->write_queue_count == 0)
		return 0;

	/* wait for write completion */
	mm_call(&io->call, MM_CALL_FLUSH, time_ms);

	int rc;
	rc = io->call.status;
	if (rc != 0) {
		mm_errno_set(rc);
		return -1;
	}

	return 0;
}
