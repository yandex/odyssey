
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>
#include <limits.h>
#include <string.h>

#include <machinarium/machinarium.h>
#include <machinarium/io.h>
#include <machinarium/bio.h>
#include <machinarium/memory.h>

/*
 * Linear write buffer, glibc stdio style (see _IO_new_file_xsputn /
 * new_do_write in libio/fileops.c).
 * The layer sits on top of the stack: reads, pending, want and error
 * checks are passed to the lower layer, writes are absorbed and
 * pushed downstream by mm_bio_write(bio->next, ...).
 */

typedef struct {
	size_t cap;
	size_t head;
	size_t tail;
	char base[];
} mm_bio_buf_state_t;

static inline int mm_bio_buf_state_is_empty(mm_bio_buf_state_t *wb)
{
	return wb->head == wb->tail;
}

/*
 * Commit the bytes consumed by the lower layer: advance the pending
 * range, reset it when drained.
 */
static inline void mm_bio_buf_state_commit(mm_bio_buf_state_t *wb, size_t size)
{
	wb->head += size;
	if (mm_bio_buf_state_is_empty(wb)) {
		wb->head = 0;
		wb->tail = 0;
	}
}

/*
 * Consume up to size bytes pushing them downstream. Returns the
 * number of bytes consumed, or -1 on error with machine errno set.
 * Never waits.
 */
static ssize_t mm_writebuf_layer_write(mm_bio_t *bio, mm_bio_buf_state_t *wb,
				       const void *src, size_t size)
{
	const char *s = src;
	size_t consumed = 0;

	while (consumed < size) {
		size_t left = size - consumed;
		size_t free = wb->cap - wb->tail;

		if (free == 0) {
			/*
			 * buffer is full: try to make room without
			 * waiting, the lower layer may still stall
			 */
			size_t processed = 0;
			ssize_t rc =
				mm_bio_write(bio->next, wb->base + wb->head,
					     wb->tail - wb->head, &processed);
			if (rc <= 0) {
				if (processed > 0) {
					/*
					 * the lower layer consumed that
					 * many buffered bytes on a
					 * partial-progress error
					 */
					mm_bio_buf_state_commit(wb, processed);
				}
				/* stalled or error */
				return consumed > 0 ? (ssize_t)consumed : -1;
			}
			mm_bio_buf_state_commit(wb, (size_t)rc);
			continue;
		}

		/*
		 * buffer is empty and there is enough data for a
		 * block-aligned direct write, bypass the buffer to
		 * avoid an extra copy (glibc _IO_new_file_xsputn);
		 * only from the empty state, so the wire order of
		 * buffered vs bypassed bytes is preserved
		 */
		if (free == wb->cap && left >= wb->cap) {
			size_t bulk = left - (left % wb->cap);
			size_t processed = 0;
			ssize_t rc = mm_bio_write(bio->next, s + consumed, bulk,
						  &processed);
			if (rc <= 0) {
				if (processed > 0) {
					consumed += processed;
				}
				return consumed > 0 ? (ssize_t)consumed : -1;
			}
			consumed += (size_t)rc;
			if ((size_t)rc < bulk) {
				/* partial write, the rest goes to the buffer */
				mm_errno_set(EAGAIN);
				continue;
			}
			continue;
		}

		size_t n = free < left ? free : left;
		memcpy(wb->base + wb->tail, s + consumed, n);
		wb->tail += n;
		consumed += n;
	}

	return (ssize_t)consumed;
}

static ssize_t mm_bio_buf_write(mm_bio_t *bio, const void *buf, size_t size,
				size_t *processed)
{
	(void)processed;
	mm_bio_buf_state_t *wb = bio->state;
	ssize_t rc = mm_writebuf_layer_write(bio, wb, buf, size);
	if (rc > 0 && mm_bio_buf_state_is_empty(wb)) {
		wb->head = 0;
		wb->tail = 0;
	}
	return rc;
}

static ssize_t mm_bio_buf_writev(mm_bio_t *bio, const struct iovec *iov,
				 int iovcnt, size_t *processed)
{
	(void)processed;
	mm_bio_buf_state_t *wb = bio->state;
	size_t consumed = 0;
	size_t skip = 0;
	int i = 0;

	if (mm_bio_buf_state_is_empty(wb)) {
		size_t total = 0;
		for (int k = 0; k < iovcnt; k++) {
			total += iov[k].iov_len;
		}
		if (total >= wb->cap) {
			/*
			 * the buffer is empty and the iovec is big:
			 * pass it through without an extra copy, the
			 * lower layer can coalesce it in one writev;
			 * small writevs keep coalescing in the buffer
			 */
			size_t p = 0;
			ssize_t rc = mm_bio_writev(bio->next, iov, iovcnt, &p);
			if (rc <= 0 && p == 0) {
				if (mm_errno_get() == 0) {
					mm_errno_set(EAGAIN);
				}
				return -1;
			}
			consumed = rc > 0 ? (size_t)rc : p;
			if (consumed == total) {
				return (ssize_t)consumed;
			}
			/* the unconsumed tail goes to the buffer */
			skip = consumed;
			while (i < iovcnt && skip >= iov[i].iov_len) {
				skip -= iov[i].iov_len;
				i++;
			}
		}
	}

	for (; i < iovcnt; i++) {
		const char *base = (const char *)iov[i].iov_base + skip;
		size_t len = iov[i].iov_len - skip;
		skip = 0;
		if (len == 0) {
			continue;
		}
		ssize_t rc = mm_writebuf_layer_write(bio, wb, base, len);
		if (rc < 0) {
			return consumed > 0 ? (ssize_t)consumed : -1;
		}
		consumed += (size_t)rc;
		if ((size_t)rc < len) {
			/* stalled in the middle of the element */
			break;
		}
	}

	if (consumed > 0 && mm_bio_buf_state_is_empty(wb)) {
		wb->head = 0;
		wb->tail = 0;
	}
	return (ssize_t)consumed;
}

static ssize_t mm_bio_buf_read(mm_bio_t *bio, void *buf, size_t size)
{
	return mm_bio_read(bio->next, buf, size);
}

static int mm_bio_buf_read_pending(mm_bio_t *bio)
{
	return mm_bio_read_pending(bio->next);
}

static int mm_bio_buf_write_pending(mm_bio_t *bio)
{
	/* number of buffered bytes, 0 when the buffer is empty */
	mm_bio_buf_state_t *wb = bio->state;
	return (int)(wb->tail - wb->head);
}

/*
 * Attempt to push the buffered data downstream without waiting, then
 * propagate the flush to the lower layers: the flush of the stack is
 * a responsibility of its top layer. Returns 0 when the stack is
 * drained, -1 with EAGAIN on a stall.
 */
static int mm_bio_buf_flush(mm_bio_t *bio)
{
	mm_bio_buf_state_t *wb = bio->state;
	while (wb->head < wb->tail) {
		size_t processed = 0;
		ssize_t rc = mm_bio_write(bio->next, wb->base + wb->head,
					  wb->tail - wb->head, &processed);
		if (rc > 0) {
			mm_bio_buf_state_commit(wb, (size_t)rc);
			continue;
		}

		if (processed > 0) {
			mm_bio_buf_state_commit(wb, processed);
			if (mm_bio_buf_state_is_empty(wb)) {
				break;
			}
		}

		/* stalled (EAGAIN) or error, the errno is already set */
		return -1;
	}

	/* drained, propagate to the lower layers */
	return mm_bio_flush(bio->next);
}

/*
 * Best effort: push whatever the buffer holds without waiting,
 * discard the rest - data may be discarded on a stall, explicit
 * mm_io_flush() is required for guaranteed delivery. Then let the
 * lower layer shut down.
 */
static int mm_bio_buf_shutdown(mm_bio_t *bio, uint32_t timeout)
{
	mm_bio_buf_state_t *wb = bio->state;
	while (wb->head < wb->tail) {
		size_t processed = 0;
		ssize_t rc = mm_bio_write(bio->next, wb->base + wb->head,
					  wb->tail - wb->head, &processed);
		if (rc <= 0) {
			/* stalled or error - discard the rest */
			break;
		}
		wb->head += (size_t)rc;
	}
	wb->head = 0;
	wb->tail = 0;
	return mm_bio_shutdown(bio->next, timeout);
}

static int mm_bio_buf_want(mm_bio_t *bio)
{
	return mm_bio_want(bio->next);
}

static const char *mm_bio_buf_error(mm_bio_t *bio)
{
	return mm_bio_error(bio->next);
}

static void mm_bio_buf_free(mm_bio_t *bio)
{
	mm_free(bio->state);
}

const mm_bio_methods_t mm_bio_buf_methods = {
	.name = "buf",
	.read = mm_bio_buf_read,
	.write = mm_bio_buf_write,
	.writev = mm_bio_buf_writev,
	.flush = mm_bio_buf_flush,
	.read_pending = mm_bio_buf_read_pending,
	.write_pending = mm_bio_buf_write_pending,
	.shutdown = mm_bio_buf_shutdown,
	.want = mm_bio_buf_want,
	.error = mm_bio_buf_error,
	.free = mm_bio_buf_free,
};

int mm_bio_buf_attach(mm_io_t *io, size_t size)
{
	mm_bio_t *slot = &io->bio_slots[MM_BIO_BUF];
	if (slot->methods != NULL) {
		mm_errno_set(EINPROGRESS);
		return -1;
	}
	if (size == 0 || size > INT_MAX) {
		/*
		 * write_pending() reports the pending count as int,
		 * larger buffers would overflow it
		 */
		mm_errno_set(EINVAL);
		return -1;
	}
	/* create first, init the slot only on success, so a failed
	 * attach leaves the slot free for a retry */
	mm_bio_buf_state_t *wb = mm_malloc(sizeof(*wb) + size);
	if (wb == NULL) {
		mm_errno_set(ENOMEM);
		return -1;
	}
	wb->cap = size;
	wb->head = 0;
	wb->tail = 0;
	mm_bio_init(slot, &mm_bio_buf_methods, io, wb);
	mm_bio_push(&io->bio, slot);
	return 0;
}
