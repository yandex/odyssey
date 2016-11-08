
/*
 * fluent.
 *
 * Cooperative multitasking engine.
*/

#include <fluent_private.h>
#include <fluent.h>

static void
ft_io_read_timeout_cb(uv_timer_t *handle)
{
	ftio *io = handle->data;
	uv_read_stop((uv_stream_t*)&io->handle);
	io->read_timeout = 1;
	io->read_status = -ETIMEDOUT;
	ft_wakeup(io->f, io->read_fiber);
}

static void
ft_io_read_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	ftio *io = handle->data;
	int to_read =
		io->read_size - ft_bufused(&io->read_buf);
	if (to_read > suggested_size)
		to_read = suggested_size;
	int rc;
	rc = ft_bufensure(&io->read_buf, to_read);
	if (rc == -1)
		return;
	buf->base = io->read_buf.p;
	buf->len  = to_read;
}

static void
ft_io_read_cb(uv_stream_t *handle, ssize_t size, const uv_buf_t *buf)
{
	ftio *io = handle->data;
	assert(! io->read_timeout);
	if (size >= 0) {
		ft_bufadvance(&io->read_buf, size);
		if (ft_bufused(&io->read_buf) < io->read_size) {
			/* expect more data */
			return;
		}
		/* read complete */
		assert(ft_bufused(&io->read_buf) == io->read_size);
	} else {
		/* connection closed */
		if (size == UV_EOF) {
			io->connected = 0;
		}
		io->read_status = size;
	}

	ft_io_timer_stop(&io->read_timer);
	uv_read_stop(handle);
	ft_wakeup(io->f, io->read_fiber);
}

FLUENT_API int
ft_read(ftio_t iop, int size, uint64_t time_ms)
{
	ftio *io = iop;
	if (!io->connected || io->read_fiber)
		return -1;
	io->read_status  = 0;
	io->read_timeout = 0;
	io->read_size    = size;
	io->read_fiber   = ft_current(io->f);
	ft_bufreset(&io->read_buf);

	ft_io_timer_start(&io->connect_timer, ft_io_read_timeout_cb,
	                  time_ms);
	int rc;
	rc = uv_read_start((uv_stream_t*)&io->handle,
	                   ft_io_read_alloc_cb,
	                   ft_io_read_cb);
	if (rc < 0) {
		io->read_fiber = NULL;
		return rc;
	}
	ft_scheduler_yield(&io->f->scheduler);
	rc = io->read_status;
	io->read_fiber = NULL;
	return rc;
}

FLUENT_API int
ft_read_is_timeout(ftio_t iop)
{
	ftio *io = iop;
	return io->read_timeout;
}

FLUENT_API char*
ft_read_buf(ftio_t iop)
{
	ftio *io = iop;
	return io->read_buf.s;
}
