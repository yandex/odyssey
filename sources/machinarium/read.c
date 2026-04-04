
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>

#include <machinarium/machinarium.h>
#include <machinarium/cond.h>
#include <machinarium/machine.h>
#include <machinarium/io.h>
#include <machinarium/fd.h>
#include <machinarium/compression.h>
#include <machinarium/tls.h>

MACHINE_API ssize_t machine_read_raw(mm_io_t *obj, void *buf, size_t size)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
#ifdef MM_BUILD_COMPRESSION
	/* If streaming compression is enabled then use correspondent compression
	 * read function. */
	if (mm_compression_is_active(io)) {
		return mm_zpq_read(io->zpq_stream, buf, size);
	}
#endif
	return mm_io_read(io, buf, size);
}

MACHINE_API int machine_read_pending(mm_io_t *io)
{
	return mm_io_read_pending(io);
}

static inline int machine_read_to(mm_io_t *io, machine_msg_t *msg, size_t size,
				  uint32_t time_ms)
{
	mm_errno_set(0);

	if (!io->attached) {
		mm_errno_set(ENOTCONN);
		return -1;
	}

	int rc;
	int offset = machine_msg_size(msg);
	rc = machine_msg_write(msg, NULL, size);
	if (rc == -1) {
		return -1;
	}

	char *dest = (char *)machine_msg_data(msg) + offset;
	size_t total = 0;
	while (total != size) {
		int rc = machine_read_raw(io, dest + total, size - total);
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
					return -1;
				}
				continue;
			}
		}

		return -1;
	}

	return 0;
}

MACHINE_API machine_msg_t *machine_read(mm_io_t *obj, size_t size,
					uint32_t time_ms)
{
	mm_errno_set(0);
	machine_msg_t *msg = machine_msg_create(0);
	if (msg == NULL) {
		return NULL;
	}
	int rc = machine_read_to(obj, msg, size, time_ms);
	if (rc == -1) {
		machine_msg_free(msg);
		return NULL;
	}
	return msg;
}
