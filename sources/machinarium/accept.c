/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>

#include <machinarium/machinarium.h>
#include <machinarium/fd.h>
#include <machinarium/io.h>
#include <machinarium/machine.h>
#include <machinarium/socket.h>

MACHINE_API int mm_io_accept(mm_io_t *obj, mm_io_t **client, int backlog,
			     int attach, uint32_t time_ms)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	int rc, fd;

	mm_errno_set(0);

	if (mm_call_is_active(&io->call)) {
		mm_errno_set(EINPROGRESS);
		return -1;
	}
	if (io->connected) {
		mm_errno_set(EINPROGRESS);
		return -1;
	}
	if (io->fd == -1) {
		mm_errno_set(EBADF);
		return -1;
	}
	if (!io->accept_listen) {
		rc = mm_socket_listen(io->fd, backlog);
		if (rc == -1) {
			mm_errno_set(errno);
			return -1;
		}
		io->accept_listen = 1;
	}
	if (!io->attached) {
		rc = mm_io_attach(io);
		if (rc == -1) {
			mm_errno_set(errno);
			return -1;
		}
	}

	mm_io_set_deadline(io, time_ms);
	while (1) {
		fd = mm_socket_accept(io->fd, NULL, NULL);
		if (fd > 0) {
			break;
		}

		int err = errno;
		if (machine_errno_retryable(err)) {
			/* wait for EPOLLIN event on socket */
			rc = mm_io_wait_deadline(io);
			if (rc != 0) {
				return -1;
			}

			continue;
		}

		mm_errno_set(err);
		return -1;
	}

	/* setup client io */
	*client = mm_io_create();
	if (*client == NULL) {
		mm_errno_set(ENOMEM);
		return -1;
	}
	mm_io_t *client_io;
	client_io = (mm_io_t *)*client;
	client_io->is_unix_socket = io->is_unix_socket;
	client_io->opt_nodelay = io->opt_nodelay;
	client_io->opt_keepalive = io->opt_keepalive;
	client_io->opt_keepalive_delay = io->opt_keepalive_delay;
	client_io->accepted = 1;
	client_io->connected = 1;
	rc = mm_io_socket_set(client_io, fd);
	if (rc == -1) {
		mm_io_close(*client);
		mm_io_free(*client);
		*client = NULL;
		return -1;
	}
	if (attach) {
		rc = mm_io_attach((mm_io_t *)client_io);
		if (rc == -1) {
			mm_io_close(*client);
			mm_io_free(*client);
			*client = NULL;
			return -1;
		}
	}
	return 0;
}
