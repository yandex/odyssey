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

int od_io_write_raw(od_io_t *io, void *buf, size_t size, size_t *processed,
		    uint32_t timeout_ms)
{
	*processed = 0;
	size_t total = 0;
	char *pos = buf;
	while (total < size) {
		int rc = machine_write_raw(io->io, pos, size - total,
					   NULL /* TODO: processed */);
		if (rc > 0) {
			total += (size_t)rc;
			*processed = total;
			continue;
		}

		int errno_ = machine_errno();
		if (errno_ == EAGAIN || errno_ == EWOULDBLOCK ||
		    errno_ == EINTR) {
			rc = mm_io_wait((mm_io_t *)io->io, timeout_ms);
			if (rc == -1) {
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
			rc = mm_io_wait((mm_io_t *)io->io, timeout_ms);
			if (rc == -1) {
				return -1;
			}
			continue;
		}

		/* error or unexpected eof */
		return -1;
	}

	abort();
}
