#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <stddef.h>
#include <sys/uio.h>

#include <machinarium/machinarium.h>
#include <machinarium/io.h>

struct mm_bio_methods {
	const char *name;
	ssize_t (*read)(mm_bio_t *, void *, size_t);
	/*
	 * write/writev return the number of bytes written. On a
	 * partial-progress error (rc <= 0) a layer may report the input
	 * bytes it already consumed via *processed; those bytes must
	 * not be resent on retry. errno is meaningful only when the
	 * call returns -1.
	 */
	ssize_t (*write)(mm_bio_t *, const void *, size_t, size_t *processed);
	ssize_t (*writev)(mm_bio_t *, const struct iovec *, int,
			  size_t *processed);
	/*
	 * Attempt to push the data a layer holds internally downstream
	 * without waiting, then propagate the flush to the lower
	 * layers. Returns 0 when the layer and everything below it is
	 * drained, -1 with EAGAIN on a stall. Layers with no internal
	 * buffering set this to NULL.
	 */
	int (*flush)(mm_bio_t *);
	int (*read_pending)(mm_bio_t *);
	int (*write_pending)(mm_bio_t *);
	int (*shutdown)(mm_bio_t *, uint32_t);
	int (*want)(mm_bio_t *);
	const char *(*error)(mm_bio_t *);
	void (*free)(mm_bio_t *);
};

extern const mm_bio_methods_t mm_bio_socket_methods;
extern const mm_bio_methods_t mm_bio_tls_methods;
extern const mm_bio_methods_t mm_bio_zpq_methods;
extern const mm_bio_methods_t mm_bio_buf_methods;

/* attach the buf layer on top of the stack */
int mm_bio_buf_attach(mm_io_t *, size_t size);

/* attach the zpq compression layer on top of the stack */
int mm_bio_zpq_attach(mm_io_t *, char algorithm);

void mm_bio_init(mm_bio_t *, const mm_bio_methods_t *, mm_io_t *, void *state);
void mm_bio_push(mm_bio_t **top, mm_bio_t *layer);
/* release the state of a single layer (methods->free) */
void mm_bio_free(mm_bio_t *);
void mm_bio_free_chain(mm_bio_t *);

static inline ssize_t mm_bio_read(mm_bio_t *bio, void *buf, size_t size)
{
	return bio->methods->read(bio, buf, size);
}

static inline ssize_t mm_bio_write(mm_bio_t *bio, const void *buf, size_t size,
				   size_t *processed)
{
	return bio->methods->write(bio, buf, size, processed);
}

static inline ssize_t mm_bio_writev(mm_bio_t *bio, const struct iovec *iov,
				    int iovcnt, size_t *processed)
{
	return bio->methods->writev(bio, iov, iovcnt, processed);
}

static inline int mm_bio_read_pending(mm_bio_t *bio)
{
	return bio->methods->read_pending(bio);
}

static inline int mm_bio_write_pending(mm_bio_t *bio)
{
	return bio->methods->write_pending(bio);
}

static inline int mm_bio_shutdown(mm_bio_t *bio, uint32_t timeout)
{
	return bio->methods->shutdown(bio, timeout);
}

static inline int mm_bio_flush(mm_bio_t *bio)
{
	if (bio->methods->flush == NULL) {
		return 0;
	}
	return bio->methods->flush(bio);
}

static inline int mm_bio_want(mm_bio_t *bio)
{
	return bio->methods->want(bio);
}

static inline const char *mm_bio_error(mm_bio_t *bio)
{
	return bio->methods->error(bio);
}
