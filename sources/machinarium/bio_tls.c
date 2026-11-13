
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>

#include <machinarium/machinarium.h>
#include <machinarium/io.h>
#include <machinarium/bio.h>
#include <machinarium/tls.h>

static ssize_t mm_bio_tls_read(mm_bio_t *bio, void *buf, size_t size)
{
	mm_io_t *io = bio->io;
	if (io->tls_ssl == NULL) {
		/* handshake is not complete yet - the layer is transparent */
		return mm_bio_read(bio->next, buf, size);
	}
	mm_errno_set(0);
	if (io->error) {
		mm_errno_set(io->error);
		return -1;
	}
	ssize_t rc;
	rc = mm_tls_read(io, buf, size);

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

static ssize_t mm_bio_tls_write(mm_bio_t *bio, const void *buf, size_t size,
				size_t *processed)
{
	(void)processed;
	mm_io_t *io = bio->io;
	if (io->tls_ssl == NULL) {
		/* handshake is not complete yet - the layer is transparent */
		return mm_bio_write(bio->next, buf, size, NULL);
	}
	mm_errno_set(0);
	ssize_t rc;
	rc = mm_tls_write(io, buf, size);
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

static ssize_t mm_bio_tls_writev(mm_bio_t *bio, const struct iovec *iov,
				 int iovcnt, size_t *processed)
{
	(void)processed;
	mm_io_t *io = bio->io;
	if (io->tls_ssl == NULL) {
		/* handshake is not complete yet - the layer is transparent */
		return mm_bio_writev(bio->next, iov, iovcnt, NULL);
	}
	ssize_t rc;
	rc = mm_tls_writev(io, iov, iovcnt);
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

static int mm_bio_tls_read_pending(mm_bio_t *bio)
{
	mm_io_t *io = bio->io;
	if (io->tls_ssl == NULL) {
		/* handshake is not complete yet - the layer is transparent */
		return mm_bio_read_pending(bio->next);
	}
	if (mm_tls_read_pending(io)) {
		return 1;
	}
	return mm_bio_read_pending(bio->next);
}

static int mm_bio_tls_write_pending(mm_bio_t *bio)
{
	return mm_bio_write_pending(bio->next);
}

static int mm_bio_tls_shutdown(mm_bio_t *bio, uint32_t timeout)
{
	mm_io_t *io = bio->io;
	int rc;
	rc = mm_tls_shutdown(io, timeout);
	if (rc == -1) {
		return -1;
	}
	return mm_bio_shutdown(bio->next, timeout);
}

static int mm_bio_tls_want(mm_bio_t *bio)
{
	mm_io_t *io = bio->io;
	return io->want;
}

static const char *mm_bio_tls_error(mm_bio_t *bio)
{
	mm_io_t *io = bio->io;
	if (io->tls_error) {
		return io->tls_error_msg;
	}
	return mm_bio_error(bio->next);
}

static void mm_bio_tls_free(mm_bio_t *bio)
{
	(void)bio;
}

const mm_bio_methods_t mm_bio_tls_methods = {
	.name = "tls",
	.read = mm_bio_tls_read,
	.write = mm_bio_tls_write,
	.writev = mm_bio_tls_writev,
	.read_pending = mm_bio_tls_read_pending,
	.write_pending = mm_bio_tls_write_pending,
	.shutdown = mm_bio_tls_shutdown,
	.want = mm_bio_tls_want,
	.error = mm_bio_tls_error,
	.free = mm_bio_tls_free,
};
