
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>

#include <machinarium/machinarium.h>
#include <machinarium/io.h>
#include <machinarium/bio.h>
#include <machinarium/zpq_stream.h>
#include <machinarium/iov.h>
#include <machinarium/memory.h>

static ssize_t mm_zpq_bio_tx(void *arg, void const *data, size_t size)
{
	mm_bio_t *bio = arg;
	return mm_bio_write(bio->next, data, size, NULL);
}

static ssize_t mm_zpq_bio_rx(void *arg, void *data, size_t size)
{
	mm_bio_t *bio = arg;
	return mm_bio_read(bio->next, data, size);
}

static ssize_t mm_bio_zpq_read(mm_bio_t *bio, void *buf, size_t size)
{
	return mm_zpq_read(bio->state, buf, size);
}

static ssize_t mm_bio_zpq_write(mm_bio_t *bio, const void *buf, size_t size,
				size_t *processed)
{
	return mm_zpq_write(bio->state, buf, size, processed);
}

static ssize_t mm_bio_zpq_writev(mm_bio_t *bio, const struct iovec *iov,
				 int iovcnt, size_t *processed)
{
	int size = mm_iov_size_of(iov, iovcnt);
	char *buffer = mm_malloc(size);
	if (buffer == NULL) {
		mm_errno_set(ENOMEM);
		return -1;
	}
	mm_iovcpy(buffer, iov, iovcnt);

	ssize_t rc;
	rc = mm_zpq_write(bio->state, buffer, size, processed);
	mm_free(buffer);
	return rc;
}

static int mm_bio_zpq_read_pending(mm_bio_t *bio)
{
	/*
	 * Count the data held by the decompressor: the next read returns it
	 * without touching the lower layer. This keeps the backpressure
	 * invariant - readahead full + inbound data available on any layer
	 * reports ENOBUFS instead of a misleading EWOULDBLOCK.
	 */
	return mm_zpq_buffered_rx(bio->state) ||
	       mm_zpq_deferred_rx(bio->state) || mm_bio_read_pending(bio->next);
}

static int mm_bio_zpq_write_pending(mm_bio_t *bio)
{
	/* number of bytes held by the compressor, 0 when empty */
	return (int)mm_zpq_buffered_tx(bio->state);
}

/*
 * Drain the bytes the compressor holds internally: every
 * mm_zpq_write() call pushes the accumulated compressed output down
 * (the return value of a zero-size write is always 0, the progress
 * is measured by the buffered count). Non-blocking: returns -1 with
 * EAGAIN on a stall.
 */
static int mm_bio_zpq_flush(mm_bio_t *bio)
{
	while (mm_zpq_buffered_tx(bio->state) != 0) {
		size_t before = mm_zpq_buffered_tx(bio->state);
		ssize_t rc = mm_zpq_write(bio->state, "", 0, NULL);
		if (rc < 0) {
			return -1;
		}
		if (mm_zpq_buffered_tx(bio->state) == before) {
			/* the transport made no progress */
			mm_errno_set(EAGAIN);
			return -1;
		}
	}
	return mm_bio_flush(bio->next);
}

static int mm_bio_zpq_shutdown(mm_bio_t *bio, uint32_t timeout)
{
	/*
	 * best effort: discard whatever the compressor still holds,
	 * data may be discarded on a stall - explicit mm_io_flush()
	 * is required for guaranteed delivery
	 */
	mm_bio_flush(bio);
	return mm_bio_shutdown(bio->next, timeout);
}

static int mm_bio_zpq_want(mm_bio_t *bio)
{
	return mm_bio_want(bio->next);
}

static const char *mm_bio_zpq_error(mm_bio_t *bio)
{
	char const *error = mm_zpq_error(bio->state);
	if (error != NULL) {
		return error;
	}
	return mm_bio_error(bio->next);
}

static void mm_bio_zpq_free(mm_bio_t *bio)
{
	mm_zpq_free(bio->state);
}

const mm_bio_methods_t mm_bio_zpq_methods = {
	.name = "zpq",
	.read = mm_bio_zpq_read,
	.write = mm_bio_zpq_write,
	.writev = mm_bio_zpq_writev,
	.flush = mm_bio_zpq_flush,
	.read_pending = mm_bio_zpq_read_pending,
	.write_pending = mm_bio_zpq_write_pending,
	.shutdown = mm_bio_zpq_shutdown,
	.want = mm_bio_zpq_want,
	.error = mm_bio_zpq_error,
	.free = mm_bio_zpq_free,
};

int mm_bio_zpq_attach(mm_io_t *io, char algorithm)
{
	if (io->bio_slots[MM_BIO_ZPQ].methods != NULL) {
		mm_errno_set(EINPROGRESS);
		return -1;
	}
	if (io->bio == &io->bio_slots[MM_BIO_BUF]) {
		/*
		 * the buf layer must stay on top of the stack,
		 * attach the compression before the writebuf
		 */
		mm_errno_set(EINPROGRESS);
		return -1;
	}
	int impl = mm_zpq_get_algorithm_impl(algorithm);
	if (impl < 0) {
		mm_errno_set(EINVAL);
		return -1;
	}
	/* create first, init the slot only on success, so a failed
	 * attach leaves the slot free for a retry */
	mm_bio_t *zpq_bio = &io->bio_slots[MM_BIO_ZPQ];
	mm_zpq_stream_t *stream = zpq_create(impl, mm_zpq_bio_tx, mm_zpq_bio_rx,
					     zpq_bio, NULL, 0);
	if (stream == NULL) {
		mm_errno_set(ENOMEM);
		return -1;
	}
	mm_bio_init(zpq_bio, &mm_bio_zpq_methods, io, stream);
	mm_bio_push(&io->bio, zpq_bio);
	return 0;
}
