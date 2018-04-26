
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

MACHINE_API machine_io_t*
machine_io_create(void)
{
	mm_errno_set(0);
	mm_io_t *io = malloc(sizeof(*io));
	if (io == NULL) {
		mm_errno_set(ENOMEM);
		return NULL;
	}
	memset(io, 0, sizeof(*io));

	/* tcp */
	io->fd = -1;
	mm_tlsio_init(&io->tls, io);

	/* read */
	io->readahead_size = 4096;
	mm_buf_init(&io->readahead_buf);
	return (machine_io_t*)io;
}

MACHINE_API void
machine_io_free(machine_io_t *obj)
{
	mm_io_t *io = mm_cast(mm_io_t*, obj);
	mm_errno_set(0);
	mm_buf_free(&io->readahead_buf);
	mm_tlsio_free(&io->tls);
	free(io);
}

MACHINE_API char*
machine_error(machine_io_t *obj)
{
	mm_io_t *io = mm_cast(mm_io_t*, obj);
	if (io->tls.error)
		return io->tls.error_msg;
	int errno_ = mm_errno_get();
	if (errno_)
		return strerror(errno_);
	return NULL;
}

MACHINE_API int
machine_fd(machine_io_t *obj)
{
	mm_io_t *io = mm_cast(mm_io_t*, obj);
	return io->fd;
}

MACHINE_API int
machine_set_nodelay(machine_io_t *obj, int enable)
{
	mm_io_t *io = mm_cast(mm_io_t*, obj);
	mm_errno_set(0);
	io->opt_nodelay = enable;
	if (io->fd != -1) {
		int rc;
		rc = mm_socket_set_nodelay(io->fd, enable);
		if (rc == -1) {
			mm_errno_set(errno);
			return -1;
		}
	}
	return 0;
}

MACHINE_API int
machine_set_keepalive(machine_io_t *obj, int enable, int delay)
{
	mm_io_t *io = mm_cast(mm_io_t*, obj);
	mm_errno_set(0);
	io->opt_keepalive = enable;
	io->opt_keepalive_delay = delay;
	if (io->fd != -1) {
		int rc;
		rc = mm_socket_set_keepalive(io->fd, enable, delay);
		if (rc == -1) {
			mm_errno_set(errno);
			return -1;
		}
	}
	return 0;
}

MACHINE_API int
machine_io_attach(machine_io_t *obj)
{
	mm_io_t *io = mm_cast(mm_io_t*, obj);
	mm_errno_set(0);
	if (io->attached) {
		mm_errno_set(EINPROGRESS);
		return -1;
	}
	int rc;
	rc = mm_loop_add(&mm_self->loop, &io->handle, 0);
	if (rc == -1) {
		mm_errno_set(errno);
		return -1;
	}
	io->attached = 1;
	return 0;
}

MACHINE_API int
machine_io_detach(machine_io_t *obj)
{
	mm_io_t *io = mm_cast(mm_io_t*, obj);
	mm_errno_set(0);
	if (! io->attached) {
		mm_errno_set(ENOTCONN);
		return -1;
	}
	int rc;
	rc = mm_loop_delete(&mm_self->loop, &io->handle);
	if (rc == -1) {
		mm_errno_set(errno);
		return -1;
	}
	io->attached = 0;
	return 0;
}

MACHINE_API int
machine_io_verify(machine_io_t *obj, char *common_name)
{
	mm_io_t *io = mm_cast(mm_io_t*, obj);
	mm_errno_set(0);
	if (io->tls_obj == NULL) {
		mm_errno_set(EINVAL);
		return -1;
	}
	int rc;
	rc = mm_tlsio_verify_common_name(&io->tls, common_name);
	return rc;
}

int mm_io_socket_set(mm_io_t *io, int fd)
{
	io->fd = fd;
	int rc;
	rc = mm_socket_set_nosigpipe(io->fd, 1);
	if (rc == -1) {
		mm_errno_set(errno);
		return -1;
	}
	rc = mm_socket_set_nonblock(io->fd, 1);
	if (rc == -1) {
		mm_errno_set(errno);
		return -1;
	}
	if (io->opt_nodelay) {
		rc = mm_socket_set_nodelay(io->fd, 1);
		if (rc == -1) {
			mm_errno_set(errno);
			return -1;
		}
	}
	if (io->opt_keepalive) {
		rc = mm_socket_set_keepalive(io->fd, 1, io->opt_keepalive_delay);
		if (rc == -1) {
			mm_errno_set(errno);
			return -1;
		}
	}
	io->handle.fd = io->fd;
	return 0;
}

int mm_io_socket(mm_io_t *io, struct sockaddr *sa)
{
	int rc;
	rc = mm_socket(sa->sa_family, SOCK_STREAM, 0);
	if (rc == -1) {
		mm_errno_set(errno);
		return -1;
	}
	return mm_io_socket_set(io, rc);
}
