
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

#if 0
static void
mm_io_free(mm_io_t *io)
{
	if (io->req_ref > 0)
		return;
	if (io->close_ref > 0)
		return;
	if (! uv_is_closing((uv_handle_t*)&io->gai_timer))
		return;
	if (! uv_is_closing((uv_handle_t*)&io->connect_timer))
		return;
	if (! uv_is_closing((uv_handle_t*)&io->read_timer))
		return;
	if (! uv_is_closing((uv_handle_t*)&io->write_timer))
		return;
	mm_buf_free(&io->read_ahead);
	free(io);
}

static void
mm_io_close_cb(uv_handle_t *handle)
{
	mm_io_t *io = handle->data;
	io->close_ref--;
	assert(io->close_ref >= 0);
	mm_io_free(io);
}

void mm_io_close_handle(mm_io_t *io, uv_handle_t *handle)
{
	if (uv_is_closing(handle))
		return;
	io->close_ref++;
	uv_close(handle, mm_io_close_cb);
}

void mm_io_req_ref(mm_io_t *io)
{
	io->req_ref++;
}

void mm_io_req_unref(mm_io_t *io)
{
	io->req_ref--;
	assert(io->req_ref >= 0);
	mm_io_free(io);
}

MACHINE_API int
machine_close(machine_io_t obj)
{
	mm_io_t *io = obj;
	mm_io_read_stop(io);
	mm_io_close_handle(io, (uv_handle_t*)&io->gai_timer);
	mm_io_close_handle(io, (uv_handle_t*)&io->connect_timer);
	mm_io_close_handle(io, (uv_handle_t*)&io->read_timer);
	mm_io_close_handle(io, (uv_handle_t*)&io->write_timer);
	mm_io_close_handle(io, (uv_handle_t*)&io->handle);
	return 0;
}
#endif

MACHINE_API int
machine_close(machine_io_t obj)
{
	mm_io_t *io = obj;
	mm_t *machine = machine = io->machine;
	if (io->fd == -1) {
		mm_io_set_errno(io, EBADF);
		return -1;
	}
	int rc;
	rc = mm_loop_delete(&machine->loop, &io->handle);
	if (rc == -1)
		mm_io_set_errno(io, errno);
	rc = close(io->fd);
	if (rc == -1)
		mm_io_set_errno(io, errno);
	io->connected = 0;
	io->fd = -1;
	io->handle.fd = -1;
	io->handle.on_read = NULL;
	io->handle.on_write = NULL;
	return 0;
}
