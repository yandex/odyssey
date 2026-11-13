
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

MACHINE_API ssize_t machine_write_raw(mm_io_t *obj, const void *buf,
				      size_t size, size_t *processed)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	return mm_io_write(io, buf, size, processed);
}

MACHINE_API ssize_t machine_writev_raw(mm_io_t *io, const struct iovec *iov,
				       int iovcnt, size_t *processed)
{
	return mm_io_writev(io, iov, iovcnt, processed);
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
	while (total != size) {
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
			if (machine_errno_retryable(errno_)) {
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
