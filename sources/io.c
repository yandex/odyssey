/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/io.h>

#include <misc.h>
#include <readahead.h>
#include <io.h>

static int write_do_duplex_step(od_io_t *io, int no_buf_fail)
{
	int rc = od_io_try_read_some(io);
	if (rc != 0) {
		int errno_ = mm_errno_get();
		if (errno_ == EWOULDBLOCK) {
			/* no data available, do nothing */
			return 0;
		}
		if (errno_ == ENOBUFS) {
			/*
			 * data is avaiable, but there is no place in readahead for it
			 * this is possible full-duplex deadlock, but for now just ignore
			 */
			return no_buf_fail;
		}

		/* some error occured - go out */
		return -1;
	}

	/* some data was read in readahead, thats good */
	return 0;
}

int od_io_write_raw(od_io_t *io, const void *buf, size_t size,
		    size_t *processed, uint32_t timeout_ms, int flags)
{
	*processed = 0;
	size_t total = 0;
	const char *pos = buf;

	mm_io_set_deadline(io->io, timeout_ms);
	while (total < size) {
		int rc = machine_write_raw(io->io, pos, size - total,
					   NULL /* TODO: processed */);
		if (rc > 0) {
			total += (size_t)rc;
			*processed = total;
			pos += rc;

			if (flags & OD_IO_WRITE_DUPLEX) {
				rc = write_do_duplex_step(io, 0);
				if (rc != 0) {
					return -1;
				}
			}
			continue;
		}

		int errno_ = machine_errno();
		if (machine_errno_retryable(errno_)) {
			if ((flags & OD_IO_WRITE_DUPLEX) &&
			    !mm_io_want_write(io->io)) {
				rc = write_do_duplex_step(io, 1);
				if (rc != 0) {
					return -1;
				}
			}
			rc = mm_io_wait_deadline(io->io);
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

	return 0;
}

/* breaks iovec arg */
static struct iovec iov_advance(struct iovec *iovec, int cnt, ssize_t size)
{
	while (cnt > 0) {
		if (iovec->iov_len > (size_t)size) {
			iovec->iov_base = (char *)iovec->iov_base + size;
			iovec->iov_len -= size;
			break;
		}

		size -= iovec->iov_len;
		++iovec;
		--cnt;
	}

	struct iovec ret;
	ret.iov_base = iovec;
	ret.iov_len = cnt;

	return ret;
}

int od_io_writev(od_io_t *io, struct iovec *iov, int iovcnt,
		 uint32_t timeout_ms, int flags)
{
	mm_io_set_deadline(io->io, timeout_ms);
	while (iovcnt > 0) {
		ssize_t rc = machine_writev_raw(io->io, iov, iovcnt);
		if (rc > 0) {
			struct iovec a = iov_advance(iov, iovcnt, rc);
			iov = a.iov_base;
			iovcnt = a.iov_len;

			if (flags & OD_IO_WRITE_DUPLEX) {
				rc = write_do_duplex_step(io, 0);
				if (rc != 0) {
					return -1;
				}
			}

			continue;
		}

		int errno_ = machine_errno();
		if (machine_errno_retryable(errno_)) {
			if ((flags & OD_IO_WRITE_DUPLEX) &&
			    !mm_io_want_write(io->io)) {
				rc = write_do_duplex_step(io, 1);
				if (rc != 0) {
					return -1;
				}
			}
			rc = mm_io_wait_deadline(io->io);
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

	return 0;
}

int od_io_read_some(od_io_t *io, uint32_t timeout_ms)
{
	mm_io_set_deadline(io->io, timeout_ms);
	while (1) {
		struct iovec vec = od_readahead_write_begin(&io->readahead);
		if (vec.iov_len == 0) {
			return -1;
		}

		int rc = machine_read_raw(io->io, vec.iov_base, vec.iov_len);
		if (rc > 0) {
			od_readahead_write_commit(&io->readahead, (size_t)rc);
			return 0;
		}

		int errno_ = machine_errno();
		if (machine_errno_retryable(errno_)) {
			rc = mm_io_wait_deadline(io->io);
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

	abort();
}

int od_io_try_read_some(od_io_t *io)
{
	struct iovec vec = od_readahead_write_begin(&io->readahead);
	if (vec.iov_len == 0) {
		if (mm_io_read_pending(io->io)) {
			mm_errno_set(ENOBUFS);
		} else {
			mm_errno_set(EWOULDBLOCK);
		}
		return -1;
	}

	int rc = machine_read_raw(io->io, vec.iov_base, vec.iov_len);
	if (rc > 0) {
		od_readahead_write_commit(&io->readahead, (size_t)rc);
		return 0;
	}

	int errno_ = machine_errno();
	if (machine_errno_retryable(errno_)) {
		mm_errno_set(EWOULDBLOCK);
		return -1;
	}

	/* error or unexpected eof */
	return -1;
}

int od_io_write_flush(od_io_t *io, uint32_t timeout_ms)
{
	const uint8_t flush[] = { KIWI_FE_FLUSH, 0, 0, 0, sizeof(uint32_t) };
	assert(sizeof(flush) == 5);
	size_t unused;
	return od_io_write_raw(io, flush, sizeof(flush), &unused, timeout_ms,
			       0);
}
