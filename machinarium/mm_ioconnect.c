
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

static void
mm_io_connect_timeout_cb(uv_timer_t *handle)
{
	mmio *io = handle->data;
	io->connect_timeout = 1;
	/* cancel connection request,
	 * connect callback will be called anyway */
	uv_handle_t *to_cancel;
	to_cancel = (uv_handle_t*)&io->handle;
	uv_close(to_cancel, NULL);
}

static void
mm_io_connect_cb(uv_connect_t *handle, int status)
{
	mmio *io = handle->data;
	if (mm_fiber_is_cancel(io->connect_fiber))
		goto wakeup;
	if (io->connect_timeout)
		goto wakeup;
	mm_io_timer_stop(&io->connect_timer);
wakeup:
	io->connect_status = status;
	mm_wakeup(io->f, io->connect_fiber);
}

static void
mm_io_connect_cancel_cb(mmfiber *fiber, void *arg)
{
	mmio *io = arg;
	io->write_timeout = 0;
	mm_io_timer_stop(&io->connect_timer);
	uv_handle_t *to_cancel;
	to_cancel = (uv_handle_t*)&io->handle;
	uv_close(to_cancel, NULL);
}

MM_API int
mm_is_connected(mmio_t iop)
{
	mmio *io = iop;
	return io->connected;
}

MM_API int
mm_connect(mmio_t iop, char *addr, int port, uint64_t time_ms)
{
	mmio *io = iop;
	mmfiber *current = mm_current(io->f);
	if (mm_fiber_is_cancel(current))
		return -ECANCELED;
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
	io->connect_fiber = current;

	/* start timer and connection */
	mm_io_timer_start(&io->connect_timer, mm_io_connect_timeout_cb,
	                  time_ms);
	rc = uv_tcp_connect(&io->connect,
	                    &io->handle,
	                    (const struct sockaddr*)&saddr, 
	                    mm_io_connect_cb);
	if (rc < 0) {
		io->connect_fiber = NULL;
		return rc;
	}

	/* register cancellation procedure */
	mm_fiber_op_begin(io->connect_fiber, mm_io_connect_cancel_cb, io);
	/* yield fiber */
	mm_scheduler_yield(&io->f->scheduler);
	mm_fiber_op_end(io->connect_fiber);

	/* result from timer or connect callback */
	rc = io->connect_status;
	if (rc == 0) {
		assert(! io->connect_timeout);
		io->connected = 1;
	}
	io->connect_fiber = NULL;
	return rc;
}

MM_API int
mm_connect_is_timeout(mmio_t iop)
{
	mmio *io = iop;
	return io->connect_timeout;
}
