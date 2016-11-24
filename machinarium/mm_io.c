
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

MM_API mmio_t
mm_io_new(mm_t envp)
{
	mm *env = envp;
	mmio *io = malloc(sizeof(*io));
	if (io == NULL)
		return NULL;
	/* tcp */
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
	mm_bufinit(&io->read_buf);
	uv_timer_init(&env->loop, &io->read_timer);
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

MM_API void
mm_close(mmio_t iop)
{
	mmio *io = iop;

	if (! uv_is_closing((uv_handle_t*)&io->connect_timer))
		uv_close((uv_handle_t*)&io->connect_timer, NULL);

	if (! uv_is_closing((uv_handle_t*)&io->read_timer))
		uv_close((uv_handle_t*)&io->read_timer, NULL);

	if (! uv_is_closing((uv_handle_t*)&io->write_timer))
		uv_close((uv_handle_t*)&io->write_timer, NULL);

	if (! uv_is_closing((uv_handle_t*)&io->handle))
		uv_close((uv_handle_t*)&io->handle, NULL);

	mm_buffree(&io->read_buf);
	free(io);
}

MM_API int
mm_fd(mmio_t iop)
{
	mmio *io = iop;
	return io->fd;
}
