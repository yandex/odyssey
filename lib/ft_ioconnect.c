
/*
 * flint.
 *
 * Cooperative multitasking engine.
*/

#include <flint_private.h>
#include <flint.h>

static void
ft_io_connect_timeout_cb(uv_timer_t *handle)
{
	ftio *io = handle->data;
	io->connect_timeout = 1;
	/* cancel connection request,
	 * connect callback will be called anyway */
	uv_handle_t *to_cancel;
	to_cancel = (uv_handle_t*)&io->handle;
	uv_close(to_cancel, NULL);
}

static void
ft_io_connect_cb(uv_connect_t *handle, int status)
{
	ftio *io = handle->data;
	if (ft_fiber_is_cancel(io->connect_fiber))
		goto wakeup;
	if (io->connect_timeout)
		goto wakeup;
	ft_io_timer_stop(&io->connect_timer);
wakeup:
	io->connect_status = status;
	ft_wakeup(io->f, io->connect_fiber);
}

static void
ft_io_connect_cancel_cb(ftfiber *fiber, void *arg)
{
	ftio *io = arg;
	ft_io_timer_stop(&io->connect_timer);
	uv_handle_t *to_cancel;
	to_cancel = (uv_handle_t*)&io->handle;
	uv_close(to_cancel, NULL);
}

FLINT_API int
ft_is_connected(ftio_t iop)
{
	ftio *io = iop;
	return io->connected;
}

FLINT_API int
ft_connect(ftio_t iop, char *addr, int port, uint64_t time_ms)
{
	ftio *io = iop;
	if (io->connect_fiber)
		return -1;
	io->connect_status  = 0;
	io->connect_timeout = 0;
	io->connect_fiber   = NULL;

	/* create connection socket */
	struct sockaddr_in saddr;
	int rc;
	rc = uv_ip4_addr(addr, port, &saddr);
	if (rc < 0)
		return rc;
	io->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (io->fd == -1) {
		io->connect_fiber = NULL;
		return -1 * errno;
	}
	rc = uv_tcp_open(&io->handle, io->fd);
	if (rc < 0) {
		io->connect_fiber = NULL;
		return rc;
	}

	/* assign fiber */
	io->connect_fiber = ft_current(io->f);

	/* start timer and connection */
	ft_io_timer_start(&io->connect_timer, ft_io_connect_timeout_cb,
	                  time_ms);
	rc = uv_tcp_connect(&io->connect,
	                    &io->handle,
	                    (const struct sockaddr*)&saddr, 
	                    ft_io_connect_cb);
	if (rc < 0) {
		io->connect_fiber = NULL;
		return rc;
	}
	/* register cancellation procedure */
	ft_fiber_opbegin(io->connect_fiber, ft_io_connect_cancel_cb, io);

	/* yield fiber */
	ft_scheduler_yield(&io->f->scheduler);
	ft_fiber_opend(io->connect_fiber);

	/* result from timer or connect callback */
	rc = io->connect_status;
	if (rc == 0) {
		assert(! io->connect_timeout);
		io->connected = 1;
	}
	io->connect_fiber = NULL;
	return rc;
}

FLINT_API int
ft_connect_is_timeout(ftio_t iop)
{
	ftio *io = iop;
	return io->connect_timeout;
}
