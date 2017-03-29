
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

static void
mm_write_timeout_cb(uv_timer_t *handle)
{
	mm_io_t *io = handle->data;
	io->write_timedout = 1;
	mm_io_close_handle(io, (uv_handle_t*)&io->write);
}

static void
mm_write_cancel_cb(void *obj, void *arg)
{
	(void)obj;
	mm_io_t *io = arg;
	io->write_timedout = 0;
	mm_io_timer_stop(&io->write_timer);
	mm_io_close_handle(io, (uv_handle_t*)&io->write);
}

static void
mm_write_cb(uv_write_t *handle, int status)
{
	mm_io_t *io = handle->data;
	if (mm_fiber_is_cancelled(io->write_fiber))
		goto wakeup;
	if (io->write_timedout)
		goto wakeup;
	mm_io_timer_stop(&io->write_timer);
wakeup:
	io->write_status = status;
	mm_scheduler_wakeup(io->write_fiber);
}

int
mm_write(mm_io_t *io, char *buf, int size, uint64_t time_ms)
{
	mm_fiber_t *current = mm_scheduler_current(&io->machine->scheduler);
	mm_io_set_errno(io, 0);
	if (mm_fiber_is_cancelled(current)) {
		mm_io_set_errno(io, ECANCELED);
		return -1;
	}
	if (io->write_fiber) {
		mm_io_set_errno(io, EINPROGRESS);
		return -1;
	}
	io->write_status   = 0;
	io->write_timedout = 0;
	io->write_fiber    = current;

	mm_io_timer_start(&io->write_timer, mm_write_timeout_cb, time_ms);
	int rc;
	uv_buf_t req = { buf, size };
	rc = uv_write(&io->write, (uv_stream_t*)&io->handle,
	              &req, 1, mm_write_cb);
	if (rc < 0) {
		mm_io_timer_stop(&io->write_timer);
		io->write_fiber = NULL;
		mm_io_set_errno_uv(io, rc);
		return -1;
	}
	mm_call_begin(&current->call, mm_write_cancel_cb, io);
	mm_scheduler_yield(&io->machine->scheduler);
	mm_call_end(&current->call);
	rc = io->write_status;
	io->write_fiber = NULL;
	if (rc < 0) {
		mm_io_set_errno_uv(io, rc);
		return -1;
	}
	return 0;
}

MACHINE_API int
machine_write(machine_io_t obj, char *buf, int size, uint64_t time_ms)
{
	mm_io_t *io = obj;
	if (mm_tls_is_active(&io->tls))
		return mm_tlsio_write(&io->tls, buf, size);
	return mm_write(io, buf, size, time_ms);
}

MACHINE_API int
machine_write_timedout(machine_io_t obj)
{
	mm_io_t *io = obj;
	return io->write_timedout;
}
