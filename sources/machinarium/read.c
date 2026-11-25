
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>
#include <limits.h>

#include <machinarium/machinarium.h>
#include <machinarium/cond.h>
#include <machinarium/machine.h>
#include <machinarium/io.h>
#include <machinarium/fd.h>
#include <machinarium/tls.h>

MACHINE_API ssize_t machine_read_raw(mm_io_t *obj, void *buf, size_t size)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	return mm_io_read(io, buf, size);
}

MACHINE_API int machine_read_pending(mm_io_t *io)
{
	return mm_io_read_pending(io);
}

MACHINE_API int machine_read_into(mm_io_t *io, void *dst, size_t size,
				  size_t *processed, uint32_t time_ms)
{
	mm_errno_set(0);
	*processed = 0;

	if (!io->attached) {
		mm_errno_set(ENOTCONN);
		return -1;
	}

	while (*processed < size) {
		/* Socket and TLS reads take an int length. */
		size_t left = size - *processed;
		ssize_t rc = machine_read_raw(io, (char *)dst + *processed,
					      left > INT_MAX ? INT_MAX : left);
		if (rc > 0) {
			*processed += rc;
			continue;
		}
		if (rc != -1 || !machine_errno_retryable(machine_errno())) {
			return -1;
		}
		if (mm_io_wait(io, time_ms) == -1) {
			return -1;
		}
	}

	return 0;
}

MACHINE_API machine_msg_t *machine_read(mm_io_t *io, size_t size,
					uint32_t time_ms)
{
	mm_errno_set(0);
	if (!io->attached) {
		mm_errno_set(ENOTCONN);
		return NULL;
	}
	if (size > INT_MAX) {
		mm_errno_set(EOVERFLOW);
		return NULL;
	}
	machine_msg_t *msg = machine_msg_create((int)size);
	if (msg == NULL) {
		return NULL;
	}
	size_t processed;
	if (machine_read_into(io, machine_msg_data(msg), size, &processed,
			      time_ms) == -1) {
		machine_msg_free(msg);
		return NULL;
	}
	return msg;
}
