
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

static void
mm_getaddrinfo_timeout_cb(uv_timer_t *handle)
{
	(void)handle;
	mmio *io = handle->data;
	io->gai_timeout = 1;
	/* cancel request,
	 * callback will be called anyway */
	uv_cancel((uv_req_t*)&io->gai);
}

static void
mm_getaddrinfo_cancel_cb(mmfiber *fiber, void *arg)
{
	mmio *io = arg;
	io->gai_timeout = 0;
	mm_io_timer_stop(io, &io->gai_timer);
	uv_cancel((uv_req_t*)&io->gai);
}

static void
mm_getaddrinfo_cb(uv_getaddrinfo_t *handle, int status, struct addrinfo *res)
{
	mmio *io = handle->data;
	if (mm_fiber_is_cancel(io->gai_fiber))
		goto wakeup;
	if (io->gai_timeout)
		goto wakeup;
	mm_io_timer_stop(io, &io->gai_timer);
wakeup:
	mm_io_req_unref(io);
	io->gai_status = status;
	io->gai_result = res;
	mm_wakeup(io->f, io->gai_fiber);
}

MM_API int
mm_getaddrinfo(mm_io_t iop, char *addr, char *service,
               struct addrinfo *hints,
               struct addrinfo **res,
               uint64_t time_ms)
{
	mmio *io = iop;
	mmfiber *current = mm_current(io->f);
	if (mm_fiber_is_cancel(current))
		return -ECANCELED;
	if (io->gai_fiber)
		return -1;
	io->gai_status  = 0;
	io->gai_timeout = 0;
	io->gai_result  = NULL;
	io->gai_fiber   = current;
	mm_io_timer_start(io, &io->gai_timer, mm_getaddrinfo_timeout_cb,
	                  time_ms);
	int rc;
	rc = uv_getaddrinfo(&io->f->loop, &io->gai, mm_getaddrinfo_cb,
	                    addr, service, hints);
	if (rc < 0) {
		mm_io_timer_stop(io, &io->gai_timer);
		io->gai_fiber = NULL;
		return rc;
	}
	mm_io_req_ref(io);
	mm_fiber_op_begin(io->gai_fiber, mm_getaddrinfo_cancel_cb, io);
	mm_scheduler_yield(&io->f->scheduler);
	mm_fiber_op_end(io->gai_fiber);
	rc = io->gai_status;
	io->gai_fiber  = NULL;
	*res = io->gai_result;
	io->gai_result = NULL;
	if (rc < 0)
		return rc;
	return 0;
}
