
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

static void
mm_accept_timer_cb(mm_timer_t *handle)
{
	mm_io_t *io = handle->arg;
	(void)io;
	io->accept_status = ETIMEDOUT;
	io->accept_timedout = 1;
	mm_scheduler_wakeup(io->accept_fiber);
}

static void
mm_accept_cancel_cb(void *obj, void *arg)
{
	mm_io_t *io = arg;
	(void)obj;
	io->accept_status = ECANCELED;
	mm_scheduler_wakeup(io->accept_fiber);
}

static void
mm_accept_on_read_cb(mm_fd_t *handle)
{
	mm_io_t *io = handle->on_read_arg;
	io->accept_status = 0;
	mm_scheduler_wakeup(io->accept_fiber);
}

static int
mm_accept(mm_io_t *io, int backlog, machine_io_t *client, uint64_t time_ms)
{
	mm_t *machine = machine = io->machine;
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
	if (io->connected) {
		mm_io_set_errno(io, EINPROGRESS);
		return -1;
	}
	if (io->fd == -1) {
		mm_io_set_errno(io, EBADF);
		return -1;
	}
	io->accept_status   = 0;
	io->accept_fiber    = NULL;
	io->accept_timedout = 0;

	int rc;
	if (! io->accept_listen) {
		rc = mm_socket_listen(io->fd, backlog);
		if (rc == -1) {
			mm_io_set_errno(io, errno);
			return -1;
		}
		io->accept_listen = 1;
	}

	/* subscribe for accept event */
	rc = mm_loop_read(&machine->loop, &io->handle,
	                  mm_accept_on_read_cb,
	                  io, 1);
	if (rc == -1) {
		mm_io_set_errno(io, errno);
		return -1;
	}

	/* wait for timedout, cancel or execution status */
	mm_timer_start(&machine->loop.clock, &io->accept_timer,
	               mm_accept_timer_cb, io, time_ms);
	mm_call_begin(&current->call, mm_accept_cancel_cb, io);
	io->accept_fiber = current;
	mm_scheduler_yield(&machine->scheduler);
	io->accept_fiber = NULL;
	mm_call_end(&current->call);
	mm_timer_stop(&io->accept_timer);

	rc = mm_loop_read(&machine->loop, &io->handle, NULL, NULL, 0);
	if (rc == -1) {
		mm_io_set_errno(io, errno);
		return -1;
	}

	rc = io->accept_status;
	if (rc != 0) {
		mm_io_set_errno(io, rc);
		return -1;
	}

	/* setup client io */
	*client = machine_create_io(io->machine);
	if (client == NULL) {
		mm_io_set_errno(io, ENOMEM);
		return -1;
	}
	mm_io_t *client_io = (mm_io_t*)*client;
	client_io->opt_nodelay = io->opt_nodelay;
	client_io->opt_keepalive = io->opt_keepalive;
	client_io->opt_keepalive_delay = io->opt_keepalive_delay;
	client_io->accepted = 1;
	client_io->connected = 1;
	rc = mm_socket_accept(io->fd, NULL, NULL);
	if (rc == -1) {
		mm_io_set_errno(io, errno);
		machine_free_io(*client);
		*client = NULL;
		return -1;
	}
	rc = mm_io_socket_set(client_io, rc);
	if (rc == -1) {
		machine_close(*client);
		machine_free_io(*client);
		*client = NULL;
		return -1;
	}
	rc = mm_loop_add(&machine->loop, &client_io->handle, 0);
	if (rc == -1) {
		mm_io_set_errno(io, errno);
		machine_close(*client);
		machine_free_io(*client);
		*client = NULL;
		return -1;
	}
	return 0;
}

MACHINE_API int
machine_accept(machine_io_t obj, machine_io_t *client, int backlog, uint64_t time_ms)
{
	mm_io_t *io = obj;
	int rc;
	rc = mm_accept(io, backlog, client, time_ms);
	if (rc == -1)
		return -1;
	return 0;
	if (! io->tls_obj)
		return 0;
	mm_io_t *io_client = *client;
	io_client->tls_obj = io->tls_obj;
	rc = mm_tlsio_accept(&io_client->tls, io->tls_obj);
	if (rc == -1) {
		io->errno_ = io_client->errno_;
		io->tls.error = io_client->tls.error;
		memcpy(io->tls.error_msg, io_client->tls.error_msg,
		       sizeof(io->tls.error_msg));

		/* todo: close */
		return -1;
	}
	return 0;
}

MACHINE_API int
machine_accept_timedout(machine_io_t obj)
{
	mm_io_t *io = obj;
	return io->accept_timedout;
}
