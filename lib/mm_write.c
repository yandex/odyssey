
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

static void
mm_io_write_timeout_cb(uv_timer_t *handle)
{
	mmio *io = handle->data;
	io->write_timeout = 1;
	mm_io_close_handle(io, (uv_handle_t*)&io->write);
}

static void
mm_io_write_cancel_cb(mmfiber *fiber, void *arg)
{
	mmio *io = arg;
	io->write_timeout = 0;
	mm_io_timer_stop(io, &io->write_timer);
	mm_io_close_handle(io, (uv_handle_t*)&io->write);
}

static void
mm_io_write_cb(uv_write_t *handle, int status)
{
	mmio *io = handle->data;
	if (mm_fiber_is_cancel(io->write_fiber))
		goto wakeup;
	if (io->write_timeout)
		goto wakeup;
	mm_io_timer_stop(io, &io->write_timer);
wakeup:
	io->write_status = status;
	mm_wakeup(io->f, io->write_fiber);
}

MM_API int
mm_write(mm_io_t iop, char *buf, int size, uint64_t time_ms)
{
	mmio *io = iop;
	mmfiber *current = mm_current(io->f);
	if (mm_fiber_is_cancel(current))
		return -ECANCELED;
	if (io->write_fiber)
		return -1;
	io->write_status  = 0;
	io->write_timeout = 0;
	io->write_fiber   = current;

	mm_io_timer_start(io, &io->write_timer, mm_io_write_timeout_cb,
	                  time_ms);
	int rc;
	uv_buf_t req = { buf, size };
	rc = uv_write(&io->write, (uv_stream_t*)&io->handle,
	              &req, 1, mm_io_write_cb);
	if (rc < 0) {
		mm_io_timer_stop(io, &io->write_timer);
		io->write_fiber = NULL;
		return rc;
	}
	mm_fiber_op_begin(io->write_fiber, mm_io_write_cancel_cb, io);
	mm_scheduler_yield(&io->f->scheduler);
	mm_fiber_op_end(io->write_fiber);
	rc = io->write_status;
	io->write_fiber = NULL;
	if (rc < 0)
		return rc;
	return 0;
}

MM_API int
mm_write_is_timeout(mm_io_t iop)
{
	mmio *io = iop;
	return io->write_timeout;
}
