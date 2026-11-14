
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>

#include <machinarium/machinarium.h>
#include <machinarium/bio.h>
#include <machinarium/io.h>
#include <machinarium/machine.h>

int mm_io_close(mm_io_t *obj)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	if (io->fd == -1) {
		mm_errno_set(EBADF);
		return -1;
	}
	if (io->attached) {
		mm_io_detach(obj);
	}
	/*
	 * Best effort: push whatever the buffered layers hold
	 * downstream without waiting; on a stall the rest is
	 * discarded, explicit mm_io_flush() is required for
	 * guaranteed delivery.
	 */
	(void)mm_bio_flush(io->bio);
	int rc;
	rc = close(io->fd);
	if (rc == -1) {
		mm_errno_set(errno);
	}
	io->connected = 0;
	io->fd = -1;
	io->handle.fd = -1;
	io->handle.on_read = NULL;
	io->handle.on_write = NULL;
	return 0;
}
