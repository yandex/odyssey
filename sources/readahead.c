/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <unistd.h>

#include <arpa/inet.h>

#include <kiwi/header.h>

#include <io.h>
#include <readahead.h>

static size_t align_by_pagesize(size_t capacity)
{
	size_t page_size = (size_t)sysconf(_SC_PAGESIZE);

	if (capacity == 0) {
		return page_size;
	}

	return (capacity + page_size - 1) & ~(page_size - 1);
}

int od_readahead_enable(od_io_t *io, int write_size, int readahead)
{
	od_readahead_t *ra = &io->readahead;
	size_t write =
		write_size > 0 ? align_by_pagesize((size_t)write_size) : 0;
	size_t read =
		align_by_pagesize((size_t)(readahead < 0 ? 0 : readahead));

	int rc = mm_io_buf_enable(io->io, write, read);
	if (rc == -1) {
		return -1;
	}

	ra->io = io->io;
	ra->size = read;
	return 0;
}

void od_readahead_free(od_readahead_t *readahead)
{
	readahead->io = NULL;
	readahead->size = 0;
}

void od_readahead_describe(od_readahead_t *readahead, char *buf,
			   size_t buf_size)
{
	if (buf_size == 0) {
		return;
	}

	buf[0] = '\0';

	if (readahead->io == NULL) {
		return;
	}

	int unread = od_readahead_unread(readahead);
	struct iovec rvec = od_readahead_read_begin(readahead);

	int written =
		snprintf(buf, buf_size, "readahead: %d bytes unread", unread);
	if (written < 0 || (size_t)written >= buf_size) {
		return;
	}

	const uint8_t *data = (const uint8_t *)rvec.iov_base;
	size_t avail = rvec.iov_len;
	size_t pos = 0;
	int msg_count = 0;

	while (pos + sizeof(kiwi_header_t) <= avail && msg_count < 10) {
		uint8_t type = data[pos];
		uint32_t len;
		memcpy(&len, data + pos + 1, sizeof(len));
		len = ntohl(len);

		if (len < 4) {
			break;
		}

		int w = snprintf(buf + written, buf_size - (size_t)written,
				 "%s%c(len=%u)",
				 msg_count == 0 ? "; msgs: " : ", ",
				 (type >= 32 && type < 127) ? (char)type : '?',
				 len);
		if (w < 0 || (size_t)w >= buf_size - (size_t)written) {
			break;
		}
		written += w;
		msg_count++;

		pos += 1 + (size_t)len;
	}
}
