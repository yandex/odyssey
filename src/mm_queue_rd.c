
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

static void
mm_queuerd_cb(mm_fd_t *handle)
{
	mm_queuerd_t *reader = handle->on_read_arg;
	uint64_t id;
	mm_socket_read(reader->fd.fd, &id, sizeof(id));
	mm_scheduler_wakeup(&mm_self->scheduler, reader->call.fiber);
}

int mm_queuerd_open(mm_queuerd_t *reader)
{
	mm_list_init(&reader->link);
	memset(&reader->call, 0, sizeof(reader->call));
	memset(&reader->fd, 0, sizeof(reader->fd));
	reader->result = NULL;
	reader->fd.fd = eventfd(0, EFD_NONBLOCK);
	if (reader->fd.fd == -1)
		return -1;
	int rc;
	rc = mm_loop_add(&mm_self->loop, &reader->fd, 0);
	if (rc == -1)
		return -1;
	rc = mm_loop_read(&mm_self->loop, &reader->fd, mm_queuerd_cb,
	                  reader, 1);
	if (rc == -1) {
		mm_loop_delete(&mm_self->loop, &reader->fd);
		return -1;
	}
	return 0;
}

void mm_queuerd_close(mm_queuerd_t *reader)
{
	if (reader->fd.fd == -1)
		return;
	mm_loop_delete(&mm_self->loop, &reader->fd);
	close(reader->fd.fd);
	reader->fd.fd = -1;
}

void mm_queuerd_notify(mm_queuerd_t *reader)
{
	uint64_t id = 1;
	mm_socket_write(reader->fd.fd, &id, sizeof(id));
}

int mm_queuerd_wait(mm_queuerd_t *reader, int time_ms)
{
	mm_call(&reader->call, time_ms);
	return reader->call.status;
}
