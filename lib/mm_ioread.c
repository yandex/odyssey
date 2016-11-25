
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

static void
mm_io_read_timeout_cb(uv_timer_t *handle)
{
	mmio *io = handle->data;
	uv_read_stop((uv_stream_t*)&io->handle);
	io->read_timeout = 1;
	io->read_status = -ETIMEDOUT;
	mm_wakeup(io->f, io->read_fiber);
}

static void
mm_io_read_cancel_cb(mmfiber *fiber, void *arg)
{
	mmio *io = arg;
	mm_io_timer_stop(io, &io->read_timer);
	uv_read_stop((uv_stream_t*)&io->handle);
	io->read_timeout = 0;
	io->read_status = -ECANCELED;
	mm_wakeup(io->f, io->read_fiber);
}

static void
mm_io_read_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	mmio *io = handle->data;
	int to_read =
		io->read_size - mm_bufused(&io->read_buf);
	if (to_read > suggested_size)
		to_read = suggested_size;
	int rc;
	rc = mm_bufensure(&io->read_buf, to_read);
	if (rc == -1)
		return;
	buf->base = io->read_buf.p;
	buf->len  = to_read;
}

static void
mm_io_read_cb(uv_stream_t *handle, ssize_t size, const uv_buf_t *buf)
{
	mmio *io = handle->data;
	assert(! io->read_timeout);
	if (size >= 0) {
		mm_bufadvance(&io->read_buf, size);
		if (mm_bufused(&io->read_buf) < io->read_size) {
			/* expect more data */
			return;
		}
		/* read complete */
		assert(mm_bufused(&io->read_buf) == io->read_size);
	} else {
		/* connection closed */
		if (size == UV_EOF) {
			io->connected = 0;
		}
		io->read_status = size;
	}

	mm_io_timer_stop(io, &io->read_timer);
	uv_read_stop(handle);
	mm_wakeup(io->f, io->read_fiber);
}

MM_API int
mm_read(mmio_t iop, int size, uint64_t time_ms)
{
	mmio *io = iop;
	mmfiber *current = mm_current(io->f);
	if (mm_fiber_is_cancel(current))
		return -ECANCELED;
	if (!io->connected || io->read_fiber)
		return -1;
	io->read_status  = 0;
	io->read_timeout = 0;
	io->read_size    = size;
	io->read_fiber   = current;
	mm_bufreset(&io->read_buf);

	mm_io_timer_start(io, &io->read_timer, mm_io_read_timeout_cb,
	                  time_ms);
	int rc;
	rc = uv_read_start((uv_stream_t*)&io->handle,
	                   mm_io_read_alloc_cb,
	                   mm_io_read_cb);
	if (rc < 0) {
		mm_io_timer_stop(io, &io->read_timer);
		io->read_fiber = NULL;
		return rc;
	}
	mm_fiber_op_begin(io->read_fiber, mm_io_read_cancel_cb, io);
	mm_scheduler_yield(&io->f->scheduler);
	mm_fiber_op_end(io->read_fiber);
	rc = io->read_status;
	io->read_fiber = NULL;
	return rc;
}

MM_API int
mm_read_is_timeout(mmio_t iop)
{
	mmio *io = iop;
	return io->read_timeout;
}

MM_API char*
mm_read_buf(mmio_t iop)
{
	mmio *io = iop;
	return io->read_buf.s;
}
