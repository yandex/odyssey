
/*
 * flint.
 *
 * Cooperative multitasking engine.
*/

#include <flint_private.h>
#include <flint.h>

FLINT_API ftio_t
ft_io_new(ft_t envp)
{
	ft *env = envp;
	ftio *io = malloc(sizeof(*io));
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
	ft_bufinit(&io->read_buf);
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

FLINT_API void
ft_close(ftio_t iop)
{
	ftio *io = iop;
	uv_timer_stop(&io->connect_timer);
	uv_timer_stop(&io->read_timer);
	uv_timer_stop(&io->write_timer);

	if (uv_is_active((uv_handle_t*)&io->connect_timer))
		uv_close((uv_handle_t*)&io->connect_timer, NULL);
	if (uv_is_active((uv_handle_t*)&io->read_timer))
		uv_close((uv_handle_t*)&io->read_timer, NULL);
	if (uv_is_active((uv_handle_t*)&io->write_timer))
		uv_close((uv_handle_t*)&io->write_timer, NULL);
	if (uv_is_active((uv_handle_t*)&io->handle))
		uv_close((uv_handle_t*)&io->handle, NULL);
	if (io->fd != -1)
		close(io->fd);

	ft_buffree(&io->read_buf);
	free(io);
}

FLINT_API int
ft_fd(ftio_t iop)
{
	ftio *io = iop;
	return io->fd;
}
