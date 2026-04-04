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

int od_io_write_raw(od_io_t *io, const void *buf, size_t size,
		    size_t *processed, uint32_t timeout_ms)
{
	*processed = 0;
	size_t total = 0;
	const char *pos = buf;
	while (total < size) {
		int rc = machine_write_raw(io->io, pos, size - total,
					   NULL /* TODO: processed */);
		if (rc > 0) {
			total += (size_t)rc;
			*processed = total;
			pos += rc;
			continue;
		}

		int errno_ = machine_errno();
		if (errno_ == EAGAIN || errno_ == EWOULDBLOCK ||
		    errno_ == EINTR) {
			rc = mm_io_wait(io->io, timeout_ms);
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
		 uint32_t timeout_ms)
{
	while (iovcnt > 0) {
		ssize_t rc = machine_writev_raw(io->io, iov, iovcnt);
		if (rc > 0) {
			struct iovec a = iov_advance(iov, iovcnt, rc);
			iov = a.iov_base;
			iovcnt = a.iov_len;

			continue;
		}

		int errno_ = machine_errno();
		if (errno_ == EAGAIN || errno_ == EWOULDBLOCK ||
		    errno_ == EINTR) {
			rc = mm_io_wait(io->io, timeout_ms);
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
		if (errno_ == EAGAIN || errno_ == EWOULDBLOCK ||
		    errno_ == EINTR) {
			rc = mm_io_wait(io->io, timeout_ms);
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
