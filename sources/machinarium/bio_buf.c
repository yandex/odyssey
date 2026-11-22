
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
 * inspired by glibc stdio buffers (see _IO_new_file_xsputn /
 * new_do_write and _IO_file_xsgetn / _IO_new_file_underflow in
 * libio/fileops.c).
 */

typedef struct {
	size_t cap;
	size_t head;
	size_t tail;
	char *base;
} mm_bio_buf_area_t;

typedef struct {
	mm_bio_buf_area_t w;
	mm_bio_buf_area_t r;
	char base[];
} mm_bio_buf_state_t;

static inline void area_init(mm_bio_buf_area_t *a, char *base, size_t size)
{
	a->cap = size;
	a->base = base;
	a->head = 0;
	a->tail = 0;
}

static inline void state_init(mm_bio_buf_state_t *st, char *base,
			      size_t readahead, size_t writebuf)
{
	/*
	 * [ readahead ... writebuf ]
	 * st->base is flexible array member
	 */
	assert(base == st->base);
	area_init(&st->r, base, readahead);
	area_init(&st->w, base + readahead, writebuf);
}

static inline int area_is_empty(mm_bio_buf_area_t *area)
{
	return area->head == area->tail;
}

static inline void area_commit(mm_bio_buf_area_t *area, size_t size)
{
	area->head += size;
	if (area_is_empty(area)) {
		area->head = 0;
		area->tail = 0;
	}
}

static inline void area_reset(mm_bio_buf_area_t *area)
{
	if (area_is_empty(area)) {
		area->head = 0;
		area->tail = 0;
	}
}

/* discard everything the area holds */
static inline void area_clear(mm_bio_buf_area_t *area)
{
	area->head = 0;
	area->tail = 0;
}

/*
 * Consume up to size bytes pushing them downstream. Returns the
 * number of bytes consumed, or -1 on error with machine errno set.
 * Never waits.
 */
static ssize_t mm_writebuf_layer_write(mm_bio_t *bio, mm_bio_buf_area_t *wb,
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
					area_commit(wb, processed);
				}
				/* stalled or error */
				return consumed > 0 ? (ssize_t)consumed : -1;
			}
			area_commit(wb, (size_t)rc);
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
	mm_bio_buf_state_t *wbs = bio->state;
	mm_bio_buf_area_t *wb = &wbs->w;

	if (wb->cap == 0) {
		return mm_bio_write(bio->next, buf, size, processed);
	}

	ssize_t rc = mm_writebuf_layer_write(bio, wb, buf, size);
	if (rc > 0 && area_is_empty(wb)) {
		wb->head = 0;
		wb->tail = 0;
	}
	return rc;
}

static ssize_t mm_bio_buf_writev(mm_bio_t *bio, const struct iovec *iov,
				 int iovcnt, size_t *processed)
{
	mm_bio_buf_state_t *wbs = bio->state;
	mm_bio_buf_area_t *wb = &wbs->w;

	if (wb->cap == 0) {
		return mm_bio_writev(bio->next, iov, iovcnt, processed);
	}

	size_t consumed = 0;
	size_t skip = 0;
	int i = 0;

	if (area_is_empty(wb)) {
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

	if (consumed > 0 && area_is_empty(wb)) {
		wb->head = 0;
		wb->tail = 0;
	}
	return (ssize_t)consumed;
}

static ssize_t mm_bio_buf_read(mm_bio_t *bio, void *buf, size_t size)
{
	mm_bio_buf_state_t *bs = bio->state;
	mm_bio_buf_area_t *r = &bs->r;

	if (r->cap == 0) {
		return mm_bio_read(bio->next, buf, size);
	}

	char *base = r->base;
	char *s = buf;
	size_t consumed = 0;

	while (consumed < size) {
		size_t left = size - consumed;
		size_t have = r->tail - r->head;

		if (have >= left) {
			/* served from the buffer */
			memcpy(s + consumed, base + r->head, left);
			r->head += left;
			consumed += left;
			continue;
		}

		if (have > 0) {
			/* drain what is buffered */
			memcpy(s + consumed, base + r->head, have);
			r->head += have;
			consumed += have;
			left -= have;
		}
		area_reset(r);

		if (left < r->cap) {
			/*
			 * the request is smaller than the buffer:
			 * underflow, refill the read area to read
			 * ahead (glibc _IO_new_file_underflow), then
			 * serve the request from the buffer
			 */
			ssize_t rc = mm_bio_read(bio->next, base, r->cap);
			if (rc <= 0) {
				/* stalled (eagain), eof or error */
				return consumed > 0 ? (ssize_t)consumed : rc;
			}
			r->head = 0;
			r->tail = (size_t)rc;
			continue;
		}

		/*
		 * the read area is empty and the request is at least
		 * a full buffer: read the block-aligned bulk directly
		 * into the user buffer to avoid an extra copy (glibc
		 * _IO_file_xsgetn), the unaligned tail is captured by
		 * the refill on the next iteration
		 */
		size_t bulk = left - (left % r->cap);
		ssize_t rc = mm_bio_read(bio->next, s + consumed, bulk);
		if (rc <= 0) {
			/* stalled (eagain), eof or error */
			return consumed > 0 ? (ssize_t)consumed : rc;
		}
		consumed += (size_t)rc;
	}

	return (ssize_t)consumed;
}

static int mm_bio_buf_read_pending(mm_bio_t *bio)
{
	mm_bio_buf_state_t *bs = bio->state;
	mm_bio_buf_area_t *rb = &bs->r;
	int buffered = (int)(rb->tail - rb->head);
	if (buffered > 0) {
		return buffered;
	}
	return mm_bio_read_pending(bio->next);
}

static int mm_bio_buf_write_pending(mm_bio_t *bio)
{
	/* number of buffered bytes, 0 when the buffer is empty */
	mm_bio_buf_state_t *bs = bio->state;
	mm_bio_buf_area_t *wb = &bs->w;
	return (int)(wb->tail - wb->head);
}

/*
 * Attempt to push the data the write area holds downstream without
 * waiting, then propagate the flush to the lower layers: the flush
 * of the stack is a responsibility of its top layer. Returns 0 when
 * the stack is drained, -1 with EAGAIN on a stall.
 */
static int mm_bio_buf_flush(mm_bio_t *bio)
{
	mm_bio_buf_state_t *bs = bio->state;
	mm_bio_buf_area_t *wb = &bs->w;
	while (wb->head < wb->tail) {
		size_t processed = 0;
		ssize_t rc = mm_bio_write(bio->next, wb->base + wb->head,
					  wb->tail - wb->head, &processed);
		if (rc > 0) {
			area_commit(wb, (size_t)rc);
			continue;
		}

		if (processed > 0) {
			area_commit(wb, processed);
			if (area_is_empty(wb)) {
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
 * Best effort: push whatever the write area holds without waiting,
 * discard the rest - data may be discarded on a stall, explicit
 * mm_io_flush() is required for guaranteed delivery.  The read area
 * is discarded, then the lower layer shuts down.
 */
static int mm_bio_buf_shutdown(mm_bio_t *bio, uint32_t timeout)
{
	mm_bio_buf_state_t *bs = bio->state;
	mm_bio_buf_area_t *rb = &bs->r;
	mm_bio_buf_area_t *wb = &bs->w;
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
	area_clear(wb);
	area_clear(rb);
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

struct iovec mm_io_buf_peek(mm_io_t *io)
{
	mm_bio_t *slot = &io->bio_slots[MM_BIO_BUF];
	if (slot->methods == NULL) {
		struct iovec empty = { NULL, 0 };
		return empty;
	}

	mm_bio_buf_state_t *bs = slot->state;
	mm_bio_buf_area_t *r = &bs->r;

	struct iovec vec;
	vec.iov_base = r->base + r->head;
	vec.iov_len = r->tail - r->head;
	return vec;
}

void mm_io_buf_peek_commit(mm_io_t *io, size_t size)
{
	mm_bio_t *slot = &io->bio_slots[MM_BIO_BUF];
	if (slot->methods == NULL) {
		return;
	}

	mm_bio_buf_state_t *bs = slot->state;
	mm_bio_buf_area_t *r = &bs->r;

	assert(size <= r->tail - r->head);
	area_commit(r, size);
}

ssize_t mm_io_buf_try_refill(mm_io_t *io)
{
	mm_bio_t *slot = &io->bio_slots[MM_BIO_BUF];
	if (slot->methods == NULL) {
		mm_errno_set(EINVAL);
		return -1;
	}

	mm_bio_buf_state_t *bs = slot->state;
	mm_bio_buf_area_t *r = &bs->r;

	if (r->cap == 0) {
		/* the read side is not buffered, nothing to pull into */
		mm_errno_set(EAGAIN);
		return -1;
	}

	size_t have = r->tail - r->head;
	if (have == 0) {
		/* fully consumed: restart from the base, same as the
		 * underflow refill in mm_bio_buf_read */
		r->head = 0;
		r->tail = 0;
	} else if (r->head > 0) {
		/*
		 * reclaim the consumed front space so the appended
		 * bytes extend the contiguous run
		 */
		memmove(r->base, r->base + r->head, have);
		r->head = 0;
		r->tail = have;
	}

	size_t free_size = r->cap - r->tail;
	if (free_size == 0) {
		/* the buffered bytes occupy the whole area */
		mm_errno_set(ENOBUFS);
		return -1;
	}

	mm_errno_set(0);
	ssize_t rc = mm_bio_read(slot->next, r->base + r->tail, free_size);
	if (rc > 0) {
		r->tail += (size_t)rc;
		return rc;
	}
	if (rc == 0) {
		/* eof from the lower layer */
		return 0;
	}
	if (mm_errno_get() == 0) {
		/* the layer failed without setting an errno */
		mm_errno_set(EAGAIN);
	}
	/* stall or error */
	return -1;
}

int mm_bio_buf_attach(mm_io_t *io, size_t write_size, size_t read_size)
{
	mm_bio_t *slot = &io->bio_slots[MM_BIO_BUF];
	if (slot->methods != NULL) {
		mm_errno_set(EINPROGRESS);
		return -1;
	}
	if (write_size == 0 && read_size == 0) {
		mm_errno_set(EINVAL);
		return -1;
	}
	if (write_size > INT_MAX || read_size > INT_MAX) {
		mm_errno_set(EINVAL);
		return -1;
	}
	/* create first, init the slot only on success, so a failed
	 * attach leaves the slot free for a retry */
	mm_bio_buf_state_t *wb =
		mm_malloc(sizeof(*wb) + write_size + read_size);
	if (wb == NULL) {
		mm_errno_set(ENOMEM);
		return -1;
	}
	state_init(wb, wb->base, read_size, write_size);
	mm_bio_init(slot, &mm_bio_buf_methods, io, wb);
	mm_bio_push(&io->bio, slot);
	return 0;
}
