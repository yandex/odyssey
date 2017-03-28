
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

static void
mm_read_timeout_cb(uv_timer_t *handle)
{
	mm_io_t *io = handle->data;
	io->read_timedout = 1;
	io->read_status = -ETIMEDOUT;
	mm_scheduler_wakeup(io->read_fiber);
}

static void
mm_read_cancel_cb(void *obj, void *arg)
{
	(void)obj;
	mm_io_t *io = arg;
	mm_io_timer_stop(&io->read_timer);
	io->read_timedout = 0;
	io->read_status = -ECANCELED;
	mm_scheduler_wakeup(io->read_fiber);
}

static void
mm_read_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	(void)suggested_size;
	mm_io_t *io = handle->data;
	buf->base = io->read_ahead.pos;
	buf->len  = mm_buf_unused(&io->read_ahead);
}

static void
mm_read_cb(uv_stream_t *handle, ssize_t size, const uv_buf_t *buf)
{
	mm_io_t *io = handle->data;
	(void)buf;

	if (size >= 0) {
		mm_buf_advance(&io->read_ahead, size);
		assert(mm_buf_used(&io->read_ahead) <= io->read_ahead_size);
		if (mm_buf_used(&io->read_ahead) == io->read_ahead_size) {
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
			mm_io_timer_stop(&io->read_timer);
			if (! io->connected)
				uv_read_stop(handle);
			mm_scheduler_wakeup(io->read_fiber);
			return;
		}
		/* readahead buffer now has atleast as minum
		 * size required by read operation */
		if (mm_buf_used(&io->read_ahead) >= io->read_size) {
			mm_io_timer_stop(&io->read_timer);
			io->read_ahead_pos = io->read_size;
			mm_scheduler_wakeup(io->read_fiber);
		}
	}
}

int
mm_read(mm_io_t *io, char *buf, int size, uint64_t time_ms)
{
	mm_fiber_t *current = mm_scheduler_current(&io->machine->scheduler);
	if (mm_fiber_is_cancelled(current))
		return -ECANCELED;
	if (io->read_fiber)
		return -1;
	if (size > io->read_ahead_size)
		return -EINVAL;
	io->read_status   = 0;
	io->read_timedout = 0;
	io->read_size     = 0;
	io->read_fiber    = NULL;

	/* allocate readahead buffer */
	int rc;
	if (! mm_buf_size(&io->read_ahead)) {
		rc = mm_buf_ensure(&io->read_ahead, io->read_ahead_size);
		if (rc == -1)
			return -1;
	}

	/* use readhead */
	int ra_left = mm_buf_used(&io->read_ahead) - io->read_ahead_pos;
	if (ra_left >= size) {
		io->read_ahead_pos_data = io->read_ahead_pos;
		io->read_ahead_pos += size;
		if (buf) {
			memcpy(buf, io->read_ahead.start + io->read_ahead_pos_data, size);
		}
		return 0;
	}

	if (! io->connected)
		return UV_EOF;

	/* readahead has insufficient data */
	if (ra_left > 0) {
		memmove(io->read_ahead.start,
		        io->read_ahead.start + io->read_ahead_pos, ra_left);
		mm_buf_reset(&io->read_ahead);
		mm_buf_advance(&io->read_ahead, ra_left);
	} else {
		mm_buf_reset(&io->read_ahead);
	}
	io->read_size           = size;
	io->read_fiber          = current;
	io->read_ahead_pos_data = 0;
	io->read_ahead_pos      = 0;

	/* subscribe fiber for new data */
	mm_io_timer_start(&io->read_timer, mm_read_timeout_cb,
	                  time_ms);
	if (! uv_is_active((uv_handle_t*)&io->handle))
	{
		rc = uv_read_start((uv_stream_t*)&io->handle,
		                   mm_read_alloc_cb,
		                   mm_read_cb);
		if (rc < 0) {
			mm_io_timer_stop(&io->read_timer);
			io->read_fiber = NULL;
			return rc;
		}
	}
	mm_call_begin(&current->call, mm_read_cancel_cb, io);
	mm_scheduler_yield(&io->machine->scheduler);
	mm_call_end(&current->call);

	io->read_fiber = NULL;
	if (mm_buf_used(&io->read_ahead) >= io->read_size) {
		rc = 0;
		if (buf) {
			memcpy(buf, io->read_ahead.start + io->read_ahead_pos_data, size);
		}
	} else {
		rc = io->read_status;
		assert(rc < 0);
	}
	return rc;
}

MACHINE_API int
machine_read(machine_io_t obj, char *buf, int size, uint64_t time_ms)
{
	mm_io_t *io = obj;
	return mm_read(io, buf, size, time_ms);
}

MACHINE_API int
machine_read_timedout(machine_io_t obj)
{
	mm_io_t *io = obj;
	return io->read_timedout;
}

MACHINE_API char*
machine_read_buf(machine_io_t obj)
{
	mm_io_t *io = obj;
	return io->read_ahead.start + io->read_ahead_pos_data;
}
