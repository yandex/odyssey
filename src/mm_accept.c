
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

static void
mm_accept_cb(uv_stream_t *handle, int status)
{
	mm_io_t *io = handle->data;
	io->accept_status = status;
	mm_scheduler_wakeup(io->accept_fiber);
}

static inline machine_io_t
mm_accept_client(mm_io_t *io)
{
	mm_io_t *client = machine_create_io(io->machine);
	if (client == NULL) {
		io->accept_status = -ENOMEM;
		return NULL;
	}
	io->accept_status =
		uv_accept((uv_stream_t*)&io->handle,
		          (uv_stream_t*)&client->handle);
	if (io->accept_status < 0) {
		machine_close(client);
		return NULL;
	}
	client->connected = 1;
	uv_fileno((uv_handle_t*)&client->handle,
	           &client->fd);
	return client;
}

MACHINE_API int
machine_accept(machine_io_t obj, int backlog, machine_io_t *client)
{
	mm_io_t *io = obj;
	mm_fiber_t *current = mm_scheduler_current(&io->machine->scheduler);
	mm_io_set_errno(io, 0);
	if (mm_fiber_is_cancelled(current)) {
		mm_io_set_errno(io, ECANCELED);
		return -1;
	}
	if (io->accept_fiber) {
		mm_io_set_errno(io, EINPROGRESS);
		return -1;
	}
	io->accept_status = 0;
	io->accept_fiber  = current;
	int rc;
	rc = uv_listen((uv_stream_t*)&io->handle, backlog, mm_accept_cb);
	if (rc < 0) {
		io->accept_fiber = NULL;
		mm_io_set_errno_uv(io, rc);
		return -1;
	}
	mm_scheduler_yield(&io->machine->scheduler);
	rc = io->accept_status;
	io->accept_fiber = NULL;
	if (rc < 0) {
		mm_io_set_errno_uv(io, rc);
		return -1;
	}
	*client = mm_accept_client(io);
	if (*client == NULL) {
		mm_io_set_errno_uv(io, io->accept_status);
		return -1;
	}
	return 0;
}
