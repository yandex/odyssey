#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium/io.h>

typedef struct od_readahead od_readahead_t;

struct od_readahead {
	mm_io_t *io;
	size_t size;
};

static inline void od_readahead_init(od_readahead_t *readahead)
{
	readahead->io = NULL;
	readahead->size = 0;
}

void od_readahead_free(od_readahead_t *readahead);

static inline int od_readahead_unread(od_readahead_t *readahead)
{
	return (int)mm_io_buf_peek(readahead->io).iov_len;
}

static inline struct iovec od_readahead_read_begin(od_readahead_t *readahead)
{
	return mm_io_buf_peek(readahead->io);
}

static inline void od_readahead_read_commit(od_readahead_t *readahead,
					    size_t count)
{
	mm_io_buf_peek_commit(readahead->io, count);
}

static inline ssize_t od_readahead_fill(od_readahead_t *readahead)
{
	return mm_io_buf_try_refill(readahead->io);
}

void od_readahead_describe(od_readahead_t *readahead, char *buf,
			   size_t buf_size);
