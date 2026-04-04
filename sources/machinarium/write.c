
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
#include <machinarium/macro.h>
#include <machinarium/socket.h>
#include <machinarium/tls.h>
#include <machinarium/compression.h>

MACHINE_API ssize_t machine_write_raw(mm_io_t *obj, const void *buf,
				      size_t size, size_t *processed)
{
	(void)processed;
	mm_io_t *io = mm_cast(mm_io_t *, obj);
#ifdef MM_BUILD_COMPRESSION
	/* If streaming compression is enabled then use correspondent compression
	 * write function. */
	if (mm_compression_is_active(io)) {
		return mm_zpq_write(io->zpq_stream, buf, size, processed);
	}
#endif
	return mm_io_write(io, buf, size);
}

MACHINE_API ssize_t machine_writev_raw(mm_io_t *io, const struct iovec *iov,
				       int iovcnt)
{
	mm_errno_set(0);

	int iov_to_write = iovcnt;
	if (iov_to_write > IOV_MAX) {
		iov_to_write = IOV_MAX;
	}

	ssize_t rc;
#ifdef MM_BUILD_COMPRESSION
	if (mm_compression_is_active(io)) {
		size_t processed = 0;
		rc = mm_compression_writev(io, iovec, iov_to_write, &processed);
		/* processed > 0 in case of error return code, but consumed input */
		if (processed > 0) {
			mm_iov_advance(iov, processed);
		}
	} else if (mm_tls_is_active(io))
#else
	if (mm_tls_is_active(io))
#endif
		rc = mm_tls_writev(io, iov, iov_to_write);
	else {
		rc = mm_socket_writev(io->fd, iov, iov_to_write);
	}

	if (rc > 0) {
		return rc;
	}
	int errno_ = errno;
	mm_errno_set(errno_);
	if (errno_ == EAGAIN || errno_ == EWOULDBLOCK || errno_ == EINTR) {
		return -1;
	}
	io->connected = 0;
	return -1;
}

MACHINE_API int machine_write_no_free(mm_io_t *destination, machine_msg_t *msg,
				      uint32_t time_ms)
{
	mm_io_t *io = mm_cast(mm_io_t *, destination);
	mm_errno_set(0);

	if (!io->attached) {
		mm_errno_set(ENOTCONN);
		goto error;
	}

	int total = 0;
	char *src = machine_msg_data(msg);
	int size = machine_msg_size(msg);
	/* If compression is on, also check that there is no data left in tx buffer
	 */
	while (total != size || mm_compression_write_pending(io)) {
		/* when using compression, some data may be processed
		 * despite the non-positive return code */
		size_t processed = 0;
		int rc = machine_write_raw(destination, src + total,
					   size - total, &processed);
		total += processed;

		if (rc > 0) {
			total += rc;
			continue;
		}

		/* error or eof */
		if (rc == -1) {
			int errno_ = machine_errno();
			if (errno_ == EAGAIN || errno_ == EWOULDBLOCK ||
			    errno_ == EINTR) {
				rc = mm_io_wait(io, time_ms);
				if (rc == -1) {
					goto error;
				}
				continue;
			}
		}

		goto error;
	}

	return 0;
error:
	return -1;
}

/* writes msg to io object.
 * Frees memory after use in current implementation
 * */
MACHINE_API int machine_write(mm_io_t *destination, machine_msg_t *msg,
			      uint32_t time_ms)
{
	int rc = machine_write_no_free(destination, msg, time_ms);
	machine_msg_free(msg);
	return rc;
}
