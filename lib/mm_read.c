
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

static void
mm_read_timeout_cb(uv_timer_t *handle)
{
	mmio *io = handle->data;
	io->read_timeout = 1;
	io->read_status = -ETIMEDOUT;
	mm_wakeup(io->f, io->read_fiber);
}

static void
mm_read_cancel_cb(mmfiber *fiber, void *arg)
{
	mmio *io = arg;
	mm_io_timer_stop(io, &io->read_timer);
	io->read_timeout = 0;
	io->read_status = -ECANCELED;
	mm_wakeup(io->f, io->read_fiber);
}

static void
mm_read_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	mmio *io = handle->data;
	buf->base = io->read_ahead.p;
	buf->len  = mm_bufunused(&io->read_ahead);
}

static void
mm_read_cb(uv_stream_t *handle, ssize_t size, const uv_buf_t *buf)
{
	mmio *io = handle->data;

	if (size >= 0) {
		mm_bufadvance(&io->read_ahead, size);
		assert(mm_bufused(&io->read_ahead) <= io->read_ahead_size);
		if (mm_bufused(&io->read_ahead) == io->read_ahead_size) {
			/* stop reader when we reach readahead limit */
			uv_read_stop(handle);
		}
	} else {
		/* connection closed */
		if (size == UV_EOF) {
			io->connected = 0;
		}
		io->read_status = size;
	}

	if (io->read_fiber) {
		if (io->read_status != 0) {
			mm_io_timer_stop(io, &io->read_timer);
			if (! io->connected)
				uv_read_stop(handle);
			mm_wakeup(io->f, io->read_fiber);
			return;
		}
		/* readahead buffer now has atleast as minum
		 * size required by read operation */
		if (mm_bufused(&io->read_ahead) >= io->read_size) {
			mm_io_timer_stop(io, &io->read_timer);
			io->read_ahead_pos = io->read_size;
			mm_wakeup(io->f, io->read_fiber);
		}
	}
}

MM_API int
mm_read(mm_io_t iop, int size, uint64_t time_ms)
{
	mmio *io = iop;
	mmfiber *current = mm_current(io->f);
	if (mm_fiber_is_cancel(current))
		return -ECANCELED;
	if (io->read_fiber)
		return -1;
	if (size > io->read_ahead_size)
		return -EINVAL;
	io->read_status  = 0;
	io->read_timeout = 0;
	io->read_size    = 0;
	io->read_fiber   = NULL;

	/* allocate readahead buffer */
	int rc;
	if (! mm_bufsize(&io->read_ahead)) {
		rc = mm_bufensure(&io->read_ahead, io->read_ahead_size);
		if (rc == -1)
			return -1;
	}

	/* use readhead */
	int ra_left = mm_bufused(&io->read_ahead) - io->read_ahead_pos;
	if (ra_left >= size) {
		io->read_ahead_pos_data = io->read_ahead_pos;
		io->read_ahead_pos += size;
		return 0;
	}

	if (! io->connected)
		return UV_EOF;

	/* readahead has insufficient data */
	if (ra_left > 0) {
		memmove(io->read_ahead.s,
		        io->read_ahead.s + io->read_ahead_pos, ra_left);
		mm_bufreset(&io->read_ahead);
		mm_bufadvance(&io->read_ahead, ra_left);
	} else {
		mm_bufreset(&io->read_ahead);
	}
	io->read_size           = size;
	io->read_fiber          = current;
	io->read_ahead_pos_data = 0;
	io->read_ahead_pos      = 0;

	/* subscribe fiber for new data */
	mm_io_timer_start(io, &io->read_timer, mm_read_timeout_cb,
	                  time_ms);
	if (! uv_is_active((uv_handle_t*)&io->handle))
	{
		rc = uv_read_start((uv_stream_t*)&io->handle,
		                   mm_read_alloc_cb,
		                   mm_read_cb);
		if (rc < 0) {
			mm_io_timer_stop(io, &io->read_timer);
			io->read_fiber = NULL;
			return rc;
		}
	}
	mm_fiber_op_begin(io->read_fiber, mm_read_cancel_cb, io);
	mm_scheduler_yield(&io->f->scheduler);
	mm_fiber_op_end(io->read_fiber);
	io->read_fiber = NULL;
	if (mm_bufused(&io->read_ahead) >= io->read_size) {
		rc = 0;
	} else {
		rc = io->read_status;
		assert(rc < 0);
	}
	return 0;
}

MM_API int
mm_read_is_timeout(mm_io_t iop)
{
	mmio *io = iop;
	return io->read_timeout;
}

MM_API char*
mm_read_buf(mm_io_t iop)
{
	mmio *io = iop;
	return io->read_ahead.s + io->read_ahead_pos_data;
}
