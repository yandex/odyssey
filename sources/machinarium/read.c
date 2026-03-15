
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

static inline int mm_read_start(mm_io_t *io, machine_cond_t *on_read)
{
	/* TODO: remove this function */
	(void)io;
	(void)on_read;
	return 0;
}

static inline int mm_read_stop(mm_io_t *io)
{
	/* TODO: remove this function */
	(void)io;
	return 0;
}

MACHINE_API int machine_read_start(machine_io_t *obj, machine_cond_t *on_read)
{
	/* TODO: remove this function */
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
	return mm_read_start(io, on_read);
}

MACHINE_API int machine_read_stop(machine_io_t *obj)
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
	return mm_read_stop(io);
}

MACHINE_API ssize_t machine_read_raw(machine_io_t *obj, void *buf, size_t size)
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

MACHINE_API int machine_read_pending(machine_io_t *obj)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	return mm_io_read_pending(io);
}

static inline int machine_read_to(machine_io_t *obj, machine_msg_t *msg,
				  size_t size, uint32_t time_ms)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	mm_errno_set(0);

	if (!io->attached) {
		mm_errno_set(ENOTCONN);
		return -1;
	}
	if (!io->connected) {
		mm_errno_set(ENOTCONN);
		return -1;
	}

	int rc;
	int offset = machine_msg_size(msg);
	rc = machine_msg_write(msg, NULL, size);
	if (rc == -1) {
		mm_read_stop(io);
		return -1;
	}

	char *dest = (char *)machine_msg_data(msg) + offset;
	size_t total = 0;
	while (total != size) {
		int rc = machine_read_raw(obj, dest + total, size - total);
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

MACHINE_API machine_msg_t *machine_read(machine_io_t *obj, size_t size,
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

MACHINE_API int machine_read_active(machine_io_t *obj)
{
	/* TODO: remove me */
	(void)obj;
	abort();
	/*
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	return io->on_read != NULL;
	*/
}
