#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium/ds/vrb.h>

typedef struct od_readahead od_readahead_t;

struct od_readahead {
	mm_virtual_rbuf_t *buf;
};

static inline void od_readahead_init(od_readahead_t *readahead)
{
	readahead->buf = NULL;
}

static inline void od_readahead_free(od_readahead_t *readahead)
{
	if (readahead->buf) {
		mm_virtual_rbuf_free(readahead->buf);
	}
}

static inline int od_readahead_prepare(od_readahead_t *readahead, int size)
{
	readahead->buf = mm_virtual_rbuf_create((size_t)size);
	if (readahead->buf == NULL) {
		return -1;
	}
	return 0;
}

static inline int od_readahead_left(od_readahead_t *readahead)
{
	assert(readahead->buf);
	return mm_virtual_rbuf_free_size(readahead->buf);
}

static inline int od_readahead_unread(od_readahead_t *readahead)
{
	return mm_virtual_rbuf_size(readahead->buf);
}

static inline int od_readahead_next_byte_is(od_readahead_t *readahead,
					    uint8_t value)
{
	struct iovec rvec = mm_virtual_rbuf_read_begin(readahead->buf);
	if (rvec.iov_len == 0) {
		/* no bytes in buf */
		return 0;
	}

	/* no read commit - just check first byte */

	return ((uint8_t *)rvec.iov_base)[0] == value;
}

static inline struct iovec od_readahead_read_begin(od_readahead_t *readahead)
{
	return mm_virtual_rbuf_read_begin(readahead->buf);
}

static inline void od_readahead_read_commit(od_readahead_t *readahead,
					    size_t count)
{
	mm_virtual_rbuf_read_commit(readahead->buf, count);
}

static inline size_t od_readahead_read(od_readahead_t *readahead, char *dest,
				       size_t max)
{
	return mm_virtual_rbuf_read(readahead->buf, dest, max);
}

static inline struct iovec od_readahead_write_begin(od_readahead_t *readahead)
{
	return mm_virtual_rbuf_write_begin(readahead->buf);
}

static inline void od_readahead_write_commit(od_readahead_t *readahead,
					     size_t count)
{
	mm_virtual_rbuf_write_commit(readahead->buf, count);
}
