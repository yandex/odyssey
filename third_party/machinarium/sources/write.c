
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
	if (io->on_write)
		mm_cond_signal((mm_cond_t *)io->on_write, &mm_self->scheduler);
}

static inline int
mm_write_start(mm_io_t *io, machine_cond_t *on_write)
{
	mm_machine_t *machine = mm_self;
	io->on_write          = on_write;
	int rc;
	rc = mm_loop_write(&machine->loop, &io->handle, mm_write_cb, io);
	if (rc == -1) {
		io->on_write = NULL;
		mm_errno_set(errno);
		return -1;
	}
	return 0;
}

static inline int
mm_write_stop(mm_io_t *io)
{
	mm_machine_t *machine = mm_self;
	io->on_write          = NULL;
	int rc;
	rc = mm_loop_write_stop(&machine->loop, &io->handle);
	return rc;
}

MACHINE_API int
machine_write_start(machine_io_t *obj, machine_cond_t *on_write)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	mm_errno_set(0);
	if (mm_call_is_active(&io->call)) {
		mm_errno_set(EINPROGRESS);
		return -1;
	}
	if (!io->attached) {
		mm_errno_set(ENOTCONN);
		return -1;
	}
	return mm_write_start(io, on_write);
}

MACHINE_API int
machine_write_stop(machine_io_t *obj)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	mm_errno_set(0);
	if (mm_call_is_active(&io->call)) {
		mm_errno_set(EINPROGRESS);
		return -1;
	}
	if (!io->attached) {
		mm_errno_set(ENOTCONN);
		return -1;
	}
	return mm_write_stop(io);
}

MACHINE_API ssize_t
machine_write_raw(machine_io_t *obj, void *buf, size_t size)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	mm_errno_set(0);
	ssize_t rc;
	if (mm_tls_is_active(io))
		rc = mm_tls_write(io, buf, size);
	else
		rc = mm_socket_write(io->fd, buf, size);
	if (rc > 0)
		return rc;
	int errno_ = errno;
	mm_errno_set(errno_);
	if (errno_ == EAGAIN || errno_ == EWOULDBLOCK || errno_ == EINTR)
		return -1;
	io->connected = 0;
	return -1;
}

MACHINE_API ssize_t
machine_writev_raw(machine_io_t *obj, machine_iov_t *obj_iov)
{
	mm_io_t *io   = mm_cast(mm_io_t *, obj);
	mm_iov_t *iov = mm_cast(mm_iov_t *, obj_iov);
	mm_errno_set(0);
	if (!mm_iov_pending(iov))
		return 0;
	struct iovec *iovec = mm_iov_pos(iov);
	int iov_to_write    = iov->iov_count;
	if (iov_to_write > IOV_MAX)
		iov_to_write = IOV_MAX;
	ssize_t rc;
	if (mm_tls_is_active(io))
		rc = mm_tls_writev(io, iovec, iov_to_write);
	else
		rc = mm_socket_writev(io->fd, iovec, iov_to_write);
	if (rc > 0) {
		mm_iov_advance(iov, rc);
		return rc;
	}
	int errno_ = errno;
	mm_errno_set(errno_);
	if (errno_ == EAGAIN || errno_ == EWOULDBLOCK || errno_ == EINTR)
		return -1;
	io->connected = 0;
	return -1;
}

MACHINE_API int
machine_write(machine_io_t *obj, machine_msg_t *msg, uint32_t time_ms)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	mm_errno_set(0);

	if (!io->attached) {
		mm_errno_set(ENOTCONN);
		goto error;
	}
	if (!io->connected) {
		mm_errno_set(ENOTCONN);
		return -1;
	}
	if (io->on_write) {
		mm_errno_set(EINPROGRESS);
		goto error;
	}

	mm_cond_t on_write;
	mm_cond_init(&on_write);
	int rc;
	rc = mm_write_start(io, (machine_cond_t *)&on_write);
	if (rc == -1)
		goto error;

	int total = 0;
	char *src = machine_msg_data(msg);
	int size  = machine_msg_size(msg);
	while (total != size) {
		rc = machine_cond_wait((machine_cond_t *)&on_write, time_ms);
		if (rc == -1) {
			mm_write_stop(io);
			goto error;
		}
		rc = machine_write_raw(obj, src + total, size - total);
		if (rc > 0) {
			total += rc;
			continue;
		}

		/* error or eof */
		if (rc == -1) {
			int errno_ = machine_errno();
			if (errno_ == EAGAIN || errno_ == EWOULDBLOCK || errno_ == EINTR)
				continue;
		}

		mm_write_stop(io);
		goto error;
	}

	mm_write_stop(io);
	machine_msg_free(msg);
	return 0;
error:
	machine_msg_free(msg);
	return -1;
}
