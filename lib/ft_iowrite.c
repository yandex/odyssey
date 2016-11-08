
/*
 * flint.
 *
 * Cooperative multitasking engine.
*/

#include <flint_private.h>
#include <flint.h>

static void
ft_io_write_timeout_cb(uv_timer_t *handle)
{
	ftio *io = handle->data;
	io->write_timeout = 1;
	uv_handle_t *to_cancel;
	to_cancel = (uv_handle_t*)&io->write;
	uv_close(to_cancel, NULL);
}

static void
ft_io_write_cb(uv_write_t *handle, int status)
{
	ftio *io = handle->data;
	if (io->write_timeout)
		goto wakeup;
	ft_io_timer_stop(&io->write_timer);
wakeup:
	io->write_status = status;
	ft_wakeup(io->f, io->write_fiber);
}

FLINT_API int
ft_write(ftio_t iop, char *buf, int size, uint64_t time_ms)
{
	ftio *io = iop;
	if (!io->connected || io->write_fiber)
		return -1;
	io->write_status  = 0;
	io->write_timeout = 0;
	io->write_fiber   = ft_current(io->f);

	ft_io_timer_start(&io->connect_timer, ft_io_write_timeout_cb,
	                  time_ms);
	int rc;
	uv_buf_t req = { buf, size };
	rc = uv_write(&io->write, (uv_stream_t*)&io->handle,
	              &req, 1, ft_io_write_cb);
	if (rc < 0) {
		io->write_fiber = NULL;
		return rc;
	}
	ft_scheduler_yield(&io->f->scheduler);
	rc = io->write_status;
	io->write_fiber = NULL;
	return rc;
}

FLINT_API int
ft_write_is_timeout(ftio_t iop)
{
	ftio *io = iop;
	return io->write_timeout;
}
