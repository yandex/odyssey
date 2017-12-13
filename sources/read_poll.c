
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

static void
mm_read_poll_cb(mm_fd_t *handle)
{
	mm_io_t *io = handle->on_read_arg;
	mm_call_t *call = io->call_poll;
	assert(call != NULL);

	call->status_data = io;
	call->status = 0;
	mm_scheduler_wakeup(&mm_self->scheduler, call->coroutine);
}

MACHINE_API machine_io_t*
machine_read_poll(machine_io_t **obj_set, int count, uint32_t time_ms)
{
	mm_io_t **io_set = mm_cast(mm_io_t**, obj_set);
	mm_io_t *io;
	mm_errno_set(0);

	if (count <= 0) {
		mm_errno_set(EINVAL);
		return NULL;
	}

	/* validate io set */
	int i;
	for (i = 0; i < count; i++)
	{
		io = io_set[i];
		if (mm_call_is_active(&io->call)) {
			mm_errno_set(EINPROGRESS);
			return NULL;
		}
		if (! io->attached) {
			mm_errno_set(ENOTCONN);
			return NULL;
		}
		/* return io if it has any pending read data */
		int ra_left = io->readahead_pos - io->readahead_pos_read;
		if (ra_left > 0)
			return (machine_io_t*)io;

		/* return io if it reached eof */
		if (! io->connected)
			return (machine_io_t*)io;
	}

	mm_call_t call;

	/* swap read handler */
	int rc;
	for (i = 0; i < count; i++)
	{
		io = io_set[i];
		io->call_poll = &call;
		rc = mm_readahead_start(io, mm_read_poll_cb, io);
		if (rc == -1) {
			while (i >= 0) {
				io = io_set[i];
				io->call_poll = NULL;
				mm_readahead_stop(io);
				i--;
			}
			return NULL;
		}
	}

	mm_call(&call, MM_CALL_READ_POLL, time_ms);

	/* check status */
	mm_io_t *ready;
	rc = io->call.status;
	if (rc == 0) {
		ready = call.status_data;
		assert(ready != NULL);
	} else {
		mm_errno_set(rc);
		ready = NULL;
	}

	/* restore read handler */
	for (i = 0; i < count; i++) {
		io = io_set[i];
		io->call_poll = NULL;
		mm_readahead_start(io, mm_readahead_cb, io);
	}

	return (machine_io_t*)ready;
}
