
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
	mm_call_t *call = io->poll_call;
	assert(call != NULL);

	if (mm_readahead_enabled(io))
		mm_readahead_cb(handle);

	io->poll_ready = 1;
	mm_scheduler_wakeup(&mm_self->scheduler, call->coroutine);
}

MACHINE_API int
machine_read_poll(machine_io_t **obj_set, machine_io_t **obj_set_ready, int count,
                  uint32_t time_ms)
{
	mm_io_t **io_set   = mm_cast(mm_io_t**, obj_set);
	mm_io_t **io_ready = mm_cast(mm_io_t**, obj_set_ready);

	mm_errno_set(0);
	if (count <= 0) {
		mm_errno_set(EINVAL);
		return -1;
	}

	/* validate io set and check for any pending events or data */
	int rc;
	int ready = 0;
	int i;
	for (i = 0; i < count; i++) {
		rc = machine_read_pending(obj_set[i]);
		if (rc == -1)
			return -1;
		if (rc > 0) {
			io_ready[ready] = io_set[i];
			ready++;
			continue;
		}
	}
	if (ready > 0)
		return ready;

	mm_call_t call;

	/* swap read handler */
	mm_io_t *io;
	for (i = 0; i < count; i++)
	{
		io = io_set[i];
		io->poll_call  = &call;
		io->poll_ready = 0;
		rc = mm_read_start(io, mm_read_poll_cb, io);
		if (rc == -1) {
			for (; i >= 0; i--) {
				io = io_set[i];
				io->poll_call  = NULL;
				io->poll_ready = 0;
				mm_read_stop(io);
			}
			return -1;
		}
	}

	mm_call(&call, MM_CALL_READ_POLL, time_ms);

	/* check status */
	rc = call.status;
	if (rc != 0) {
		mm_errno_set(rc);
		for (i = 0; i < count; i++) {
			io = io_set[i];
			io->poll_call  = NULL;
			io->poll_ready = 0;
			mm_read_stop(io);
		}
		return -1;
	}

	/* restore read handler and fill ready set */
	for (i = 0; i < count; i++) {
		io = io_set[i];
		if (io->poll_ready) {
			io_ready[ready] = io;
			ready++;
		}
		io->poll_call  = NULL;
		io->poll_ready = 0;
		if (mm_readahead_enabled(io))
			mm_read_start(io, mm_readahead_cb, io);
		else
			mm_read_stop(io);
	}
	return ready;
}
