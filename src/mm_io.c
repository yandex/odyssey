
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

MACHINE_API machine_io_t
machine_create_io(machine_t obj)
{
	mm_t *machine = obj;
	mm_io_t *io = malloc(sizeof(*io));
	if (io == NULL)
		return NULL;
	/* tcp */
	io->close_ref = 0;
	io->req_ref = 0;
	io->tls_obj = NULL;
	io->errno_ = 0;
	mm_tlsio_init(&io->tls, io);
	io->machine = machine;
	uv_tcp_init(&machine->loop, &io->handle);
	io->handle.data = io;
	/* getaddrinfo */
	memset(&io->gai, 0, sizeof(io->gai));
	uv_timer_init(&machine->loop, &io->gai_timer);
	io->gai.data = io;
	io->gai_timer.data = io;
	io->gai_fiber = NULL;
	io->gai_status = 0;
	io->gai_timedout = 0;
	io->gai_result = NULL;
	/* connect */
	memset(&io->connect, 0, sizeof(io->connect));
	uv_timer_init(&machine->loop, &io->connect_timer);
	io->connect.data = io;
	io->connect_timer.data = io;
	io->connect_timedout = 0;
	io->connect_status = 0;
	io->connected  = 0;
	io->connect_fiber = NULL;
	/* accept */
	io->accept_status = 0;
	io->accept_fiber = NULL;
	/* read */
	mm_buf_init(&io->read_ahead);
	uv_timer_init(&machine->loop, &io->read_timer);
	io->read_ahead_size = 16384;
	io->read_ahead_pos = 0;
	io->read_ahead_pos_data = 0;
	io->read_timer.data = io;
	io->read_size = 0;
	io->read_status = 0;
	io->read_timedout = 0;
	io->read_eof = 0;
	io->read_fiber = NULL;
	/* write */
	memset(&io->write, 0, sizeof(io->write));
	uv_timer_init(&machine->loop, &io->write_timer);
	io->write.data = io;
	io->write_timer.data = io;
	io->write_timedout = 0;
	io->write_fiber = NULL;
	io->write_status = 0;
	return io;
}

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

MACHINE_API void
machine_close(machine_io_t obj)
{
	mm_io_t *io = obj;
	mm_io_read_stop(io);
	mm_io_close_handle(io, (uv_handle_t*)&io->gai_timer);
	mm_io_close_handle(io, (uv_handle_t*)&io->connect_timer);
	mm_io_close_handle(io, (uv_handle_t*)&io->read_timer);
	mm_io_close_handle(io, (uv_handle_t*)&io->write_timer);
	mm_io_close_handle(io, (uv_handle_t*)&io->handle);
}

MACHINE_API void
machine_set_tls(machine_io_t obj, machine_tls_t tls_obj)
{
	mm_io_t *io = obj;
	io->tls_obj = tls_obj;
}

MACHINE_API int
machine_errno(machine_io_t obj)
{
	mm_io_t *io = obj;
	return io->errno_;
}

MACHINE_API char*
machine_error(machine_io_t obj)
{
	mm_io_t *io = obj;
	(void)io;
	return NULL;
}

MACHINE_API int
machine_fd(machine_io_t obj)
{
	mm_io_t *io = obj;
	int fd;
	int rc = uv_fileno((uv_handle_t*)&io->handle, &fd);
	if (rc < 0) {
		mm_io_set_errno_uv(io, rc);
		return -1;
	}
	return fd;
}

MACHINE_API int
machine_set_nodelay(machine_io_t obj, int enable)
{
	mm_io_t *io = obj;
	int rc = uv_tcp_nodelay(&io->handle, enable);
	if (rc < 0) {
		mm_io_set_errno_uv(io, rc);
		return -1;
	}
	return 0;
}

MACHINE_API int
machine_set_keepalive(machine_io_t obj, int enable, int delay)
{
	mm_io_t *io = obj;
	int rc = uv_tcp_keepalive(&io->handle, enable, delay);
	if (rc < 0) {
		mm_io_set_errno_uv(io, rc);
		return -1;
	}
	return 0;
}

MACHINE_API int
machine_set_readahead(machine_io_t obj, int size)
{
	mm_io_t *io = obj;
	if (mm_buf_size(&io->read_ahead) > 0) {
		mm_io_set_errno(io, EINPROGRESS);
		return -1;
	}
	io->read_ahead_size = size;
	return 0;
}
