
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

MACHINE_API machine_io_t
machine_create_io(machine_t obj)
{
	mm_machine_t *machine = obj;
	mm_io_t *io = malloc(sizeof(*io));
	if (io == NULL)
		return NULL;
	memset(io, 0, sizeof(*io));

	/* tcp */
	io->fd = -1;
	mm_tlsio_init(&io->tls, io);
	io->machine = machine;

	/* read */
	mm_buf_init(&io->readahead_buf);

#if 0
	/* getaddrinfo */
	memset(&io->gai, 0, sizeof(io->gai));
	uv_timer_init(&machine->loop, &io->gai_timer);
	io->gai.data = io;
	io->gai_timer.data = io;
	io->gai_fiber = NULL;
	io->gai_status = 0;
	io->gai_timedout = 0;
	io->gai_result = NULL;
#endif
	return io;
}

MACHINE_API void
machine_free_io(machine_io_t obj)
{
	mm_io_t *io = obj;
	mm_buf_free(&io->readahead_buf);
	free(io);
}

MACHINE_API int
machine_errno(machine_io_t obj)
{
	mm_io_t *io = obj;
	return io->errno_;
}

MACHINE_API char*
machine_error(machine_io_t obj)
{
	mm_io_t *io = obj;
	if (io->tls.error)
		return io->tls.error_msg;
	if (io->errno_)
		return strerror(io->errno_);
	return NULL;
}

MACHINE_API int
machine_fd(machine_io_t obj)
{
	mm_io_t *io = obj;
	return io->fd;
}

MACHINE_API int
machine_set_nodelay(machine_io_t obj, int enable)
{
	mm_io_t *io = obj;
	io->opt_nodelay = enable;
	if (io->fd != -1) {
		int rc;
		rc = mm_socket_set_nodelay(io->fd, enable);
		if (rc == -1) {
			mm_io_set_errno(io, errno);
			return -1;
		}
	}
	return 0;
}

MACHINE_API int
machine_set_keepalive(machine_io_t obj, int enable, int delay)
{
	mm_io_t *io = obj;
	io->opt_keepalive = enable;
	io->opt_keepalive_delay = delay;
	if (io->fd != -1) {
		int rc;
		rc = mm_socket_set_keepalive(io->fd, enable, delay);
		if (rc == -1) {
			mm_io_set_errno(io, errno);
			return -1;
		}
	}
	return 0;
}

int mm_io_socket_set(mm_io_t *io, int fd)
{
	io->fd = fd;
	int rc;
	rc = mm_socket_set_nosigpipe(io->fd, 1);
	if (rc == -1) {
		mm_io_set_errno(io, errno);
		return -1;
	}
	rc = mm_socket_set_nonblock(io->fd, 1);
	if (rc == -1) {
		mm_io_set_errno(io, errno);
		return -1;
	}
	if (io->opt_nodelay) {
		rc = mm_socket_set_nodelay(io->fd, 1);
		if (rc == -1) {
			mm_io_set_errno(io, errno);
			return -1;
		}
	}
	if (io->opt_keepalive) {
		rc = mm_socket_set_keepalive(io->fd, 1, io->opt_keepalive_delay);
		if (rc == -1) {
			mm_io_set_errno(io, errno);
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
		mm_io_set_errno(io, errno);
		return -1;
	}
	return mm_io_socket_set(io, rc);
}
