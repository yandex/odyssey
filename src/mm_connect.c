
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

static void
mm_connect_timeout_cb(uv_timer_t *handle)
{
	mm_io_t *io = handle->data;
	io->connect_timedout = 1;
	/* cancel connection request,
	 * connect callback will be called anyway */
	mm_io_close_handle(io, (uv_handle_t*)&io->handle);
}

static void
mm_connect_cb(uv_connect_t *handle, int status)
{
	mm_io_t *io = handle->data;
	if (mm_fiber_is_cancelled(io->connect_fiber))
		goto wakeup;
	if (io->connect_timedout)
		goto wakeup;
	mm_io_timer_stop(&io->connect_timer);
wakeup:
	io->connect_status = status;
	mm_scheduler_wakeup(io->connect_fiber);
}

static void
mm_connect_cancel_cb(void *obj, void *arg)
{
	(void)obj;
	mm_io_t *io = arg;
	io->write_timedout = 0;
	mm_io_timer_stop(&io->connect_timer);
	mm_io_close_handle(io, (uv_handle_t*)&io->handle);
}

MACHINE_API int
machine_connect(machine_io_t obj, struct sockaddr *sa, uint64_t time_ms)
{
	mm_io_t *io = obj;
	mm_t *machine = machine = io->machine;
	mm_fiber_t *current = mm_scheduler_current(&machine->scheduler);
	if (mm_fiber_is_cancelled(current))
		return -ECANCELED;
	if (io->connect_fiber)
		return -1;
	io->connect_status   = 0;
	io->connect_timedout = 0;
	io->connect_fiber    = current;

	/* start timer and connection */
	mm_io_timer_start(&io->connect_timer, mm_connect_timeout_cb, time_ms);
	int rc;
	rc = uv_tcp_connect(&io->connect, &io->handle,
	                    sa, mm_connect_cb);
	if (rc < 0) {
		mm_io_timer_stop(&io->connect_timer);
		io->connect_fiber = NULL;
		return rc;
	}

	/* wait for completion */
	mm_call_begin(&current->call, mm_connect_cancel_cb, io);
	mm_scheduler_yield(&machine->scheduler);
	mm_call_end(&current->call);

	/* result from timer or connect callback */
	rc = io->connect_status;
	if (rc == 0) {
		assert(! io->connect_timedout);
		io->connected = 1;
	}
	io->connect_fiber = NULL;
	return rc;
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
