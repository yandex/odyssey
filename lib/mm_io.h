#ifndef MM_IO_H_
#define MM_IO_H_

/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

typedef struct mmio mmio;

struct mmio {
	uv_os_sock_t      fd;
	uv_tcp_t          handle;
	int               close_ref;
	int               cancel_ref;
	mm               *f;
	/* getaddrinfo */
	uv_getaddrinfo_t  gai;
	uv_timer_t        gai_timer;
	mmfiber          *gai_fiber;
	int               gai_status;
	int               gai_timeout;
	struct addrinfo  *gai_result;
	/* connect */
	uv_connect_t      connect;
	uv_timer_t        connect_timer;
	int               connect_timeout;
	int               connected;
	int               connect_status;
	mmfiber          *connect_fiber;
	/* accept */
	int               accept_status;
	mmfiber          *accept_fiber;
	/* read */
	uv_timer_t        read_timer;
	int               read_ahead_size;
	mmbuf             read_ahead;
	int               read_ahead_pos;
	int               read_ahead_pos_data;
	int               read_size;
	int               read_timeout;
	int               read_eof;
	int               read_status;
	mmfiber          *read_fiber;
	/* write */
	uv_write_t        write;
	uv_timer_t        write_timer;
	int               write_timeout;
	int               write_status;
	mmfiber          *write_fiber;
};

void mm_io_cancel_req(mmio*, uv_req_t*);
void mm_io_close_handle(mmio*, uv_handle_t*);

void mm_io_on_cancel_req(mmio*);

static inline void
mm_io_timer_start(mmio *io, uv_timer_t *timer, uv_timer_cb callback, uint64_t time_ms)
{
	(void)io;
	if (time_ms > 0)
		uv_timer_start(timer, callback, time_ms, 0);
}

static inline void
mm_io_timer_stop(mmio *io, uv_timer_t *timer)
{
	(void)io;
	uv_timer_stop(timer);
}

#endif
