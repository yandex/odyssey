
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

MACHINE_API int
machine_getsockname(machine_io_t obj, struct sockaddr *sa, int *salen)
{
	mm_io_t *io = obj;
	int rc = uv_tcp_getsockname(&io->handle, sa, salen);
	if (rc < 0)
		return rc;
	return 0;
}

MACHINE_API int
machine_getpeername(machine_io_t obj, struct sockaddr *sa, int *salen)
{
	mm_io_t *io = obj;
	int rc = uv_tcp_getpeername(&io->handle, sa, salen);
	if (rc < 0)
		return rc;
	return 0;
}

static void
mm_getaddrinfo_timeout_cb(uv_timer_t *handle)
{
	(void)handle;
	mm_io_t *io = handle->data;
	io->gai_timedout = 1;
	/* cancel request,
	 * callback will be called anyway */
	uv_cancel((uv_req_t*)&io->gai);
}

static void
mm_getaddrinfo_cancel_cb(void *obj, void *arg)
{
	(void)obj;
	mm_io_t *io = arg;
	io->gai_timedout = 0;
	mm_io_timer_stop(&io->gai_timer);
	uv_cancel((uv_req_t*)&io->gai);
}

static void
mm_getaddrinfo_cb(uv_getaddrinfo_t *handle, int status, struct addrinfo *res)
{
	mm_io_t *io = handle->data;
	if (mm_fiber_is_cancelled(io->gai_fiber))
		goto wakeup;
	if (io->gai_timedout)
		goto wakeup;
	mm_io_timer_stop(&io->gai_timer);
wakeup:
	mm_io_req_unref(io);
	io->gai_status = status;
	io->gai_result = res;
	mm_scheduler_wakeup(io->gai_fiber);
}

MACHINE_API int
machine_getaddrinfo(machine_io_t obj, char *addr, char *service,
                    struct addrinfo *hints,
                    struct addrinfo **res,
                    uint64_t time_ms)
{
	mm_io_t *io = obj;
	mm_fiber_t *current = mm_scheduler_current(&io->machine->scheduler);
	if (mm_fiber_is_cancelled(current))
		return -ECANCELED;
	if (io->gai_fiber)
		return -1;
	io->gai_status   = 0;
	io->gai_timedout = 0;
	io->gai_result   = NULL;
	io->gai_fiber    = current;
	mm_io_timer_start(&io->gai_timer, mm_getaddrinfo_timeout_cb, time_ms);
	int rc;
	rc = uv_getaddrinfo(&io->machine->loop, &io->gai, mm_getaddrinfo_cb,
	                    addr, service, hints);
	if (rc < 0) {
		mm_io_timer_stop(&io->gai_timer);
		io->gai_fiber = NULL;
		return rc;
	}
	mm_io_req_ref(io);
	mm_call_begin(&current->call, mm_getaddrinfo_cancel_cb, io);
	mm_scheduler_yield(&io->machine->scheduler);
	mm_call_end(&current->call);
	rc = io->gai_status;
	io->gai_fiber  = NULL;
	*res = io->gai_result;
	io->gai_result = NULL;
	if (rc < 0)
		return rc;
	return 0;
}
