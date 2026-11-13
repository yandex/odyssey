
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>
#include <string.h>
#include <sys/socket.h>

#include <machinarium/machinarium.h>
#include <machinarium/io.h>
#include <machinarium/bio.h>
#include <machinarium/socket.h>

static ssize_t mm_bio_socket_read(mm_bio_t *bio, void *buf, size_t size)
{
	mm_io_t *io = bio->io;
	mm_errno_set(0);
	if (io->error) {
		mm_errno_set(io->error);
		return -1;
	}
	ssize_t rc;
	rc = mm_socket_read(io->fd, buf, size);

	if (rc > 0) {
		/* do not set read_ready to 0 - there can be more bytes in socket */
		return rc;
	}
	io->read_ready = 0;
	if (rc < 0) {
		int errno_ = errno;
		mm_errno_set(errno_);
		if (machine_errno_retryable(errno_)) {
			return -1;
		}
	}
	/* error or eof */
	io->connected = 0;
	return rc;
}

static ssize_t mm_bio_socket_write(mm_bio_t *bio, const void *buf, size_t size,
				   size_t *processed)
{
	(void)processed;
	mm_io_t *io = bio->io;
	mm_errno_set(0);
	ssize_t rc;
	rc = mm_socket_write(io->fd, buf, size);
	if (rc > 0) {
		return rc;
	}
	int errno_ = errno;
	mm_errno_set(errno_);
	if (machine_errno_retryable(errno_)) {
		return -1;
	}
	io->connected = 0;
	return -1;
}

static ssize_t mm_bio_socket_writev(mm_bio_t *bio, const struct iovec *iov,
				    int iovcnt, size_t *processed)
{
	(void)processed;
	mm_io_t *io = bio->io;
	ssize_t rc;
	rc = mm_socket_writev(io->fd, iov, iovcnt);
	if (rc > 0) {
		return rc;
	}
	int errno_ = errno;
	mm_errno_set(errno_);
	if (machine_errno_retryable(errno_)) {
		return -1;
	}
	io->connected = 0;
	return -1;
}

static int mm_bio_socket_read_pending(mm_bio_t *bio)
{
	mm_io_t *io = bio->io;
	return mm_socket_read_pending(io->fd);
}

static int mm_bio_socket_write_pending(mm_bio_t *bio)
{
	(void)bio;
	return 0;
}

static int mm_bio_socket_shutdown(mm_bio_t *bio, uint32_t timeout)
{
	(void)timeout;
	mm_io_t *io = bio->io;
	mm_errno_set(0);

	int rc = shutdown(io->fd, SHUT_RDWR);

	if (rc == -1) {
		return MM_NOTOK_RETCODE;
	}

	return MM_OK_RETCODE;
}

static int mm_bio_socket_want(mm_bio_t *bio)
{
	(void)bio;
	return 0;
}

static const char *mm_bio_socket_error(mm_bio_t *bio)
{
	mm_io_t *io = bio->io;
	if (io->error) {
		return strerror(io->error);
	}
	return NULL;
}

static void mm_bio_socket_free(mm_bio_t *bio)
{
	(void)bio;
}

const mm_bio_methods_t mm_bio_socket_methods = {
	.name = "socket",
	.read = mm_bio_socket_read,
	.write = mm_bio_socket_write,
	.writev = mm_bio_socket_writev,
	.read_pending = mm_bio_socket_read_pending,
	.write_pending = mm_bio_socket_write_pending,
	.shutdown = mm_bio_socket_shutdown,
	.want = mm_bio_socket_want,
	.error = mm_bio_socket_error,
	.free = mm_bio_socket_free,
};
