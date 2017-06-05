
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

static void
mm_condition_cb(mm_fd_t *handle)
{
	mm_condition_t *condition = handle->on_read_arg;
	uint64_t id;
	int rc;
	rc = mm_socket_read(condition->fd.fd, &id, sizeof(id));
	(void)rc;
	assert(rc == sizeof(id));
	mm_scheduler_wakeup(&mm_self->scheduler, condition->call.coroutine);
}

int mm_condition_open(mm_condition_t *condition)
{
	mm_list_init(&condition->link);
	memset(&condition->call, 0, sizeof(condition->call));
	memset(&condition->fd, 0, sizeof(condition->fd));
	condition->fd.fd = eventfd(0, EFD_NONBLOCK);
	if (condition->fd.fd == -1)
		return -1;
	int rc;
	rc = mm_loop_add(&mm_self->loop, &condition->fd, 0);
	if (rc == -1)
		return -1;
	rc = mm_loop_read(&mm_self->loop, &condition->fd,
	                  mm_condition_cb, condition);
	if (rc == -1) {
		mm_loop_delete(&mm_self->loop, &condition->fd);
		return -1;
	}
	return 0;
}

void mm_condition_close(mm_condition_t *condition)
{
	if (condition->fd.fd == -1)
		return;
	mm_loop_delete(&mm_self->loop, &condition->fd);
	close(condition->fd.fd);
	condition->fd.fd = -1;
}

void mm_condition_signal(int fd)
{
	uint64_t id = 1;
	int rc;
	rc = mm_socket_write(fd, &id, sizeof(id));
	(void)rc;
	assert(rc == sizeof(id));
}

int mm_condition_wait(mm_condition_t *condition, int time_ms)
{
	mm_call(&condition->call, MM_CALL_CONDITION, time_ms);
	return condition->call.status;
}
