
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

MM_API mm_io_t
mm_io_new(mm_t envp)
{
	mm *env = envp;
	mmio *io = malloc(sizeof(*io));
	if (io == NULL)
		return NULL;
	/* tcp */
	io->close_ref = 0;
	io->fd = -1;
	io->f = env;
	uv_tcp_init(&env->loop, &io->handle);
	io->handle.data = io;
	/* connect */
	memset(&io->connect, 0, sizeof(io->connect));
	uv_timer_init(&env->loop, &io->connect_timer);
	io->connect.data = io;
	io->connect_timer.data = io;
	io->connect_timeout = 0;
	io->connect_status = 0;
	io->connected  = 0;
	io->connect_fiber = NULL;
	/* accept */
	io->accept_status = 0;
	io->accept_fiber = NULL;
	/* read */
	mm_bufinit(&io->read_ahead);
	uv_timer_init(&env->loop, &io->read_timer);
	io->read_ahead_size = 16384;
	io->read_ahead_pos = 0;
	io->read_ahead_pos_data = 0;
	io->read_timer.data = io;
	io->read_size = 0;
	io->read_status = 0;
	io->read_timeout = 0;
	io->read_eof = 0;
	io->read_fiber = NULL;
	/* write */
	memset(&io->write, 0, sizeof(io->write));
	uv_timer_init(&env->loop, &io->write_timer);
	io->write.data = io;
	io->write_timer.data = io;
	io->write_timeout = 0;
	io->write_fiber = NULL;
	io->write_status = 0;
	return io;
}

static void
mm_io_close_cb(uv_handle_t *handle)
{
	mmio *io = handle->data;
	io->close_ref--;
	assert(io->close_ref >= 0);
	if (io->close_ref > 0)
		return;
	if (! uv_is_closing((uv_handle_t*)&io->handle))
		return;
	if (! uv_is_closing((uv_handle_t*)&io->connect_timer))
		return;
	if (! uv_is_closing((uv_handle_t*)&io->read_timer))
		return;
	if (! uv_is_closing((uv_handle_t*)&io->write_timer))
		return;
	mm_buffree(&io->read_ahead);
	free(io);
}

void mm_io_close_handle(mmio *io, uv_handle_t *handle)
{
	if (uv_is_closing(handle))
		return;
	io->close_ref++;
	uv_close(handle, mm_io_close_cb);
}

MM_API void
mm_close(mm_io_t iop)
{
	mmio *io = iop;
	mm_io_read_stop(io);
	mm_io_close_handle(io, (uv_handle_t*)&io->connect_timer);
	mm_io_close_handle(io, (uv_handle_t*)&io->read_timer);
	mm_io_close_handle(io, (uv_handle_t*)&io->write_timer);
	mm_io_close_handle(io, (uv_handle_t*)&io->handle);
}

MM_API int
mm_io_fd(mm_io_t iop)
{
	mmio *io = iop;
	return io->fd;
}

MM_API int
mm_io_nodelay(mm_io_t iop, int enable)
{
	mmio *io = iop;
	int rc = uv_tcp_nodelay(&io->handle, enable);
	return rc;
}

MM_API int
mm_io_keepalive(mm_io_t iop, int enable, int delay)
{
	mmio *io = iop;
	int rc = uv_tcp_keepalive(&io->handle, enable, delay);
	return rc;
}

MM_API int
mm_io_readahead(mm_io_t iop, int size)
{
	mmio *io = iop;
	if (mm_bufsize(&io->read_ahead) > 0)
		return -1;
	io->read_ahead_size = size;
	return 0;
}
