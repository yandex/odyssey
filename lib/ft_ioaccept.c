
/*
 * flint.
 *
 * Cooperative multitasking engine.
*/

#include <flint_private.h>
#include <flint.h>

static void
ft_io_accept_cb(uv_stream_t *handle, int status)
{
	ftio *io = handle->data;
	io->accept_status = status;
	ft_wakeup(io->f, io->accept_fiber);
}

static inline ftio_t
ft_io_accept_client(ftio *io)
{
	ftio *client = (ftio*)ft_io_new(io->f);
	if (client == NULL) {
		io->accept_status = -ENOMEM;
		return NULL;
	}
	io->accept_status =
		uv_accept((uv_stream_t*)&io->handle,
		          (uv_stream_t*)&client->handle);
	if (io->accept_status < 0) {
		ft_close(client);
		return NULL;
	}
	client->connected = 1;
	uv_fileno((uv_handle_t*)&client->handle,
	           &client->fd);
	return client;
}

FLINT_API int
ft_accept(ftio_t iop, ftio_t *client)
{
	ftio *io = iop;
	if (io->accept_fiber)
		return -1;
	io->accept_status = 0;
	io->accept_fiber  = ft_current(io->f);
	int rc;
	rc = uv_listen((uv_stream_t*)&io->handle, 128, ft_io_accept_cb);
	if (rc < 0) {
		io->accept_fiber = NULL;
		return rc;
	}
	ft_scheduler_yield(&io->f->scheduler);
	rc = io->accept_status;
	io->accept_fiber = NULL;
	if (rc < 0)
		return rc;
	*client = ft_io_accept_client(io);
	if (*client == NULL)
		rc = io->accept_status;
	return rc;
}
