#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

/*
 * Structure for scatter/gather I/O
 * */

#include <sys/uio.h>

static inline int mm_iov_size_of(const struct iovec *iov, int count)
{
	int size = 0;
	while (count > 0) {
		size += iov->iov_len;
		iov++;
		count--;
	}
	return size;
}

static inline void mm_iovcpy(char *dest, const struct iovec *iov, int count)
{
	const struct iovec *pos = iov;
	int pos_dest = 0;
	int n = count;
	while (n > 0) {
		memcpy(dest + pos_dest, pos->iov_base, pos->iov_len);
		pos_dest += pos->iov_len;
		pos++;
		n--;
	}
}

static inline void mm_iovncpy(char *dest, const struct iovec *vec, int max,
			      int count)
{
	/* same as mm_iovcpy but copy no more than max bytes */
	while (count > 0 && max > 0) {
		int len = (int)vec->iov_len;
		if (len > max) {
			len = max;
		}

		memcpy(dest, vec->iov_base, len);

		max -= len;
		dest += len;
		--count;
		++vec;
	}
}
