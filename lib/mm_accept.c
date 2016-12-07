
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

static void
mm_io_accept_cb(uv_stream_t *handle, int status)
{
	mmio *io = handle->data;
	io->accept_status = status;
	mm_wakeup(io->f, io->accept_fiber);
}

static inline mm_io_t
mm_io_accept_client(mmio *io)
{
	mmio *client = (mmio*)mm_io_new(io->f);
	if (client == NULL) {
		io->accept_status = -ENOMEM;
		return NULL;
	}
	io->accept_status =
		uv_accept((uv_stream_t*)&io->handle,
		          (uv_stream_t*)&client->handle);
	if (io->accept_status < 0) {
		mm_close(client);
		return NULL;
	}
	client->connected = 1;
	uv_fileno((uv_handle_t*)&client->handle,
	           &client->fd);
	return client;
}

MM_API int
mm_accept(mm_io_t iop, int backlog, mm_io_t *client)
{
	mmio *io = iop;
	mmfiber *current = mm_current(io->f);
	if (mm_fiber_is_cancel(current))
		return -ECANCELED;
	if (io->accept_fiber)
		return -1;
	io->accept_status = 0;
	io->accept_fiber  = current;
	int rc;
	rc = uv_listen((uv_stream_t*)&io->handle, backlog, mm_io_accept_cb);
	if (rc < 0) {
		io->accept_fiber = NULL;
		return rc;
	}
	mm_scheduler_yield(&io->f->scheduler);
	rc = io->accept_status;
	io->accept_fiber = NULL;
	if (rc < 0)
		return rc;
	*client = mm_io_accept_client(io);
	if (*client == NULL)
		rc = io->accept_status;
	return rc;
}
