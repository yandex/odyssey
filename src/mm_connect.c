
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

static void
mm_connect_timer_cb(mm_timer_t *handle)
{
	mm_io_t *io = handle->arg;
	io->connect_status = ETIMEDOUT;
	io->connect_timedout = 1;
	mm_scheduler_wakeup(io->connect_fiber);
}

static void
mm_connect_cancel_cb(void *obj, void *arg)
{
	mm_io_t *io = arg;
	(void)obj;
	io->connect_status = ECANCELED;
	mm_scheduler_wakeup(io->connect_fiber);
}

static int
mm_connect_on_write_cb(mm_fd_t *handle)
{
	mm_io_t *io = handle->on_write_arg;
	io->connect_status = mm_socket_error(handle->fd);
	mm_scheduler_wakeup(io->connect_fiber);
	return 0;
}

static int
mm_connect(mm_io_t *io, struct sockaddr *sa, uint64_t time_ms)
{
	mm_t *machine = machine = io->machine;
	mm_fiber_t *current = mm_scheduler_current(&machine->scheduler);
	mm_io_set_errno(io, 0);

	if (mm_fiber_is_cancelled(current)) {
		mm_io_set_errno(io, ECANCELED);
		return -1;
	}
	if (io->connect_fiber) {
		mm_io_set_errno(io, EINPROGRESS);
		return -1;
	}
	if (io->connected) {
		mm_io_set_errno(io, EINPROGRESS);
		return -1;
	}
	io->connect_status   = 0;
	io->connect_timedout = 0;
	io->connect_fiber    = NULL;

	/* create socket */
	int rc;
	rc = mm_io_socket(io, sa);
	if (rc == -1)
		goto error;

	/* start connection */
	rc = mm_socket_connect(io->fd, sa);
	if (rc == 0)
		goto done;

	assert(rc == -1);
	if (errno != EINPROGRESS) {
		mm_io_set_errno(io, errno);
		goto error;
	}

	/* add socket to event loop */
	rc = mm_loop_add(&machine->loop, &io->handle, MM_W);
	if (rc == -1) {
		mm_io_set_errno(io, errno);
		goto error;
	}

	/* subscribe for connection event */
	rc = mm_loop_write(&machine->loop, &io->handle,
	                   mm_connect_on_write_cb,
	                   io, 1);
	if (rc == -1) {
		mm_io_set_errno(io, errno);
		goto error;
	}

	/* wait for timedout, cancel or execution status */
	mm_timer_start(&machine->loop.clock, &io->connect_timer,
	               mm_connect_timer_cb, io, time_ms);
	mm_call_begin(&current->call, mm_connect_cancel_cb, io);
	io->connect_fiber = current;
	mm_scheduler_yield(&machine->scheduler);
	io->connect_fiber = NULL;
	mm_call_end(&current->call);
	mm_timer_stop(&io->connect_timer);

	rc = mm_loop_write(&machine->loop, &io->handle, NULL, NULL, 0);
	if (rc == -1) {
		mm_io_set_errno(io, errno);
		goto error;
	}

	rc = io->connect_status;
	if (rc != 0) {
		mm_io_set_errno(io, rc);
		goto error;
	}

done:
	assert(! io->connect_timedout);
	io->connected = 1;
	return 0;

error:
	if (io->fd != -1) {
		close(io->fd);
		io->fd = -1;
	}
	io->handle.fd = -1;
	return -1;
}

MACHINE_API int
machine_connect(machine_io_t obj, struct sockaddr *sa, uint64_t time_ms)
{
	mm_io_t *io = obj;
	int rc = mm_connect(io, sa, time_ms);
	if (rc == -1)
		return -1;
	if (! io->tls_obj)
		return 0;
#if 0
	rc = mm_tlsio_connect(&io->tls, io->tls_obj);
	if (rc == -1) {
		/* todo: close */
		return -1;
	}
#endif
	return 0;
}

MACHINE_API int
machine_connect_timedout(machine_io_t obj)
{
	mm_io_t *io = obj;
	return io->connect_timedout;
}

MACHINE_API int
machine_connected(machine_io_t obj)
{
	mm_io_t *io = obj;
	return io->connected;
}
