
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

static void
mm_accept_on_read_cb(mm_fd_t *handle)
{
	mm_io_t *io = handle->on_read_arg;
	mm_call_t *call = &io->call;
	if (mm_call_is_aborted(call))
		return;
	call->status = 0;
	mm_scheduler_wakeup(&mm_self->scheduler, call->coroutine);
}

static int
mm_accept(mm_io_t *io, int backlog, machine_io_t *client, uint32_t time_ms)
{
	mm_machine_t *machine = mm_self;
	mm_errno_set(0);

	if (mm_call_is_active(&io->call)) {
		mm_errno_set(EINPROGRESS);
		return -1;
	}
	if (io->connected) {
		mm_errno_set(EINPROGRESS);
		return -1;
	}
	if (io->fd == -1) {
		mm_errno_set(EBADF);
		return -1;
	}
	assert(io->attached);

	int rc;
	if (! io->accept_listen) {
		rc = mm_socket_listen(io->fd, backlog);
		if (rc == -1) {
			mm_errno_set(errno);
			return -1;
		}
		io->accept_listen = 1;
	}

	/* subscribe for accept event */
	rc = mm_loop_read(&machine->loop, &io->handle,
	                  mm_accept_on_read_cb,
	                  io);
	if (rc == -1) {
		mm_errno_set(errno);
		return -1;
	}

	/* wait for completion */
	mm_call(&io->call, MM_CALL_ACCEPT, time_ms);

	rc = mm_loop_read_stop(&machine->loop, &io->handle);
	if (rc == -1) {
		mm_errno_set(errno);
		return -1;
	}

	rc = io->call.status;
	if (rc != 0) {
		mm_errno_set(rc);
		return -1;
	}

	/* setup client io */
	*client = machine_io_create();
	if (client == NULL) {
		mm_errno_set(ENOMEM);
		return -1;
	}
	mm_io_t *client_io;
	client_io = (mm_io_t*)*client;
	client_io->opt_nodelay = io->opt_nodelay;
	client_io->opt_keepalive = io->opt_keepalive;
	client_io->opt_keepalive_delay = io->opt_keepalive_delay;
	client_io->accepted = 1;
	client_io->connected = 1;
	rc = mm_socket_accept(io->fd, NULL, NULL);
	if (rc == -1) {
		mm_errno_set(errno);
		machine_io_free(*client);
		*client = NULL;
		return -1;
	}
	rc = mm_io_socket_set(client_io, rc);
	if (rc == -1) {
		machine_close(*client);
		machine_io_free(*client);
		*client = NULL;
		return -1;
	}
	rc = machine_io_attach(client_io);
	if (rc == -1) {
		machine_close(*client);
		machine_io_free(*client);
		*client = NULL;
		return -1;
	}
	return 0;
}

MACHINE_API int
machine_accept(machine_io_t obj, machine_io_t *client,
               int backlog, uint32_t time_ms)
{
	mm_io_t *io = obj;
	int rc;
	rc = mm_accept(io, backlog, client, time_ms);
	if (rc == -1)
		return -1;
	if (! io->tls_obj)
		return 0;
	mm_io_t *io_client = *client;
	io_client->tls_obj = io->tls_obj;
	rc = mm_tlsio_accept(&io_client->tls, io->tls_obj);
	if (rc == -1) {
		io->tls.error = io_client->tls.error;
		memcpy(io->tls.error_msg, io_client->tls.error_msg,
		       sizeof(io->tls.error_msg));

		/* todo: close */
		return -1;
	}
	return 0;
}
