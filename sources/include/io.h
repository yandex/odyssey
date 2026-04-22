#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi/kiwi.h>

#include <machinarium/fd.h>
#include <machinarium/io.h>

#include <readahead.h>

typedef struct od_io od_io_t;

struct od_io {
	od_readahead_t readahead;
	mm_io_t *io;
};

static inline void od_io_init(od_io_t *io)
{
	io->io = NULL;
	od_readahead_init(&io->readahead);
}

static inline void od_io_free(od_io_t *io)
{
	od_readahead_free(&io->readahead);
}

static inline char *od_io_error(od_io_t *io)
{
	return mm_io_error(io->io);
}

static inline int od_io_prepare(od_io_t *io, mm_io_t *io_obj)
{
	int rc;
	rc = od_readahead_prepare(&io->readahead);
	if (rc == -1) {
		return -1;
	}

	io->io = io_obj;

	return 0;
}

static inline int od_io_close(od_io_t *io)
{
	if (io->io == NULL) {
		return -1;
	}
	int rc = mm_io_close(io->io);
	mm_io_free(io->io);
	io->io = NULL;
	return rc;
}

static inline int od_io_attach(od_io_t *io)
{
	return mm_io_attach(io->io);
}

static inline int od_io_detach(od_io_t *io)
{
	return mm_io_detach(io->io);
}

static inline int od_io_poll(od_io_t *io)
{
	return mm_io_poll(io->io);
}

static inline void od_io_set_peer(od_io_t *io, od_io_t *peer)
{
	mm_io_set_peer((mm_io_t *)io->io, (mm_io_t *)peer->io);
}

static inline void od_io_remove_peer(od_io_t *io, od_io_t *peer)
{
	mm_io_remove_peer((mm_io_t *)io->io, (mm_io_t *)peer->io);
}

static inline int od_io_read(od_io_t *io, char *dest, int size,
			     uint32_t time_ms)
{
	int pos = 0;
	int rc;
	for (;;) {
		int nread = (int)od_readahead_read(&io->readahead, dest + pos,
						   (size_t)size);
		size -= nread;
		pos += nread;

		if (size == 0) {
			break;
		}

		for (;;) {
			struct iovec vec =
				od_readahead_write_begin(&io->readahead);

			rc = machine_read_raw(io->io, vec.iov_base,
					      vec.iov_len);
			if (rc <= 0) {
				/* retry using read condition wait */
				int errno_ = machine_errno();
				if (machine_errno_retryable(errno_)) {
					rc = mm_io_wait((mm_io_t *)io->io,
							time_ms);
					if (rc == MM_COND_WAIT_FAIL) {
						/* io wait will set errno to ETIMEDOUT or ECANCELLED */
						return -1;
					}
					if (rc == MM_COND_WAIT_OK_PROPAGATED) {
						mm_errno_set(EAGAIN);
						return -1;
					}
					continue;
				}
				/* error or unexpected eof */
				return -1;
			}

			od_readahead_write_commit(&io->readahead, (size_t)rc);
			break;
		}
	}

	return 0;
}

static inline machine_msg_t *od_read_startup(od_io_t *io, uint32_t time_ms)
{
	uint32_t header;
	int rc;
	rc = od_io_read(io, (char *)&header, sizeof(header), time_ms);
	if (rc == -1) {
		return NULL;
	}

	/* pre-validate startup header size, actual header parsing will be done by
	 * kiwi_be_read_startup() */
	uint32_t size;
	rc = kiwi_validate_startup_header((char *)&header, sizeof(header),
					  &size);
	if (rc == -1) {
		return NULL;
	}

	machine_msg_t *msg;
	msg = machine_msg_create(sizeof(header) + size);
	if (msg == NULL) {
		return NULL;
	}

	char *dest;
	dest = machine_msg_data(msg);
	memcpy(dest, &header, sizeof(header));
	dest += sizeof(header);

	rc = od_io_read(io, dest, size, time_ms);
	if (rc == -1) {
		machine_msg_free(msg);
		return NULL;
	}

	return msg;
}

static inline machine_msg_t *od_read(od_io_t *io, uint32_t time_ms)
{
	kiwi_header_t header;
	int rc;
	rc = od_io_read(io, (char *)&header, sizeof(header), time_ms);
	if (rc == -1) {
		return NULL;
	}

	/* pre-validate packet header */
	uint32_t size;
	rc = kiwi_validate_header((char *)&header, sizeof(header), &size);
	if (rc == -1) {
		return NULL;
	}
	size -= sizeof(uint32_t);

	machine_msg_t *msg;
	msg = machine_msg_create(sizeof(header) + size);
	if (msg == NULL) {
		return NULL;
	}

	char *dest;
	dest = machine_msg_data(msg);
	memcpy(dest, &header, sizeof(header));
	dest += sizeof(header);

	rc = od_io_read(io, dest, size, time_ms);
	if (rc == -1) {
		machine_msg_free(msg);
		return NULL;
	}

	return msg;
}

static inline int od_write(od_io_t *io, machine_msg_t *msg)
{
	return machine_write(io->io, msg, UINT32_MAX);
}

int od_io_read_some(od_io_t *io, uint32_t timeout_ms);

/*
 * Note: this function do not frees msg
 */
static inline int od_io_write(od_io_t *io, machine_msg_t *msg,
			      uint32_t timeout_ms)
{
	return machine_write_no_free(io->io, msg, timeout_ms);
}

static inline int od_io_connected(od_io_t *io)
{
	return mm_io_connected(io->io);
}

int od_io_write_raw(od_io_t *io, const void *buf, size_t size,
		    size_t *processed, uint32_t timeout_ms);

/* breaks iovec arg */
int od_io_writev(od_io_t *io, struct iovec *iov, int iovcnt,
		 uint32_t timeout_ms);

static inline int od_io_last_event(od_io_t *io)
{
	return mm_io_last_event(io->io);
}
