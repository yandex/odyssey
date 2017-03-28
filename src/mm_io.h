#ifndef MM_IO_H_
#define MM_IO_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_io_t mm_io_t;

struct mm_io_t {
	uv_os_sock_t      fd;
	uv_tcp_t          handle;
	mm_tlsio_t        tls;
	mm_tls_t         *tls_obj;
	int               close_ref;
	int               req_ref;
	mm_t             *machine;
	/* getaddrinfo */
	uv_getaddrinfo_t  gai;
	uv_timer_t        gai_timer;
	mm_fiber_t       *gai_fiber;
	int               gai_status;
	int               gai_timedout;
	struct addrinfo  *gai_result;
	/* connect */
	uv_connect_t      connect;
	uv_timer_t        connect_timer;
	int               connect_timedout;
	int               connected;
	int               connect_status;
	mm_fiber_t       *connect_fiber;
	/* accept */
	int               accept_status;
	mm_fiber_t       *accept_fiber;
	/* read */
	uv_timer_t        read_timer;
	int               read_ahead_size;
	mm_buf_t          read_ahead;
	int               read_ahead_pos;
	int               read_ahead_pos_data;
	int               read_size;
	int               read_timedout;
	int               read_eof;
	int               read_status;
	mm_fiber_t       *read_fiber;
	/* write */
	uv_write_t        write;
	uv_timer_t        write_timer;
	int               write_timedout;
	int               write_status;
	mm_fiber_t       *write_fiber;
};

void mm_io_req_ref(mm_io_t*);
void mm_io_req_unref(mm_io_t*);

void mm_io_close_handle(mm_io_t*, uv_handle_t*);

static inline void
mm_io_timer_start(uv_timer_t *timer,
                  uv_timer_cb callback, uint64_t time_ms)
{
	if (time_ms > 0)
		uv_timer_start(timer, callback, time_ms, 0);
}

static inline void
mm_io_timer_stop(uv_timer_t *timer)
{
	uv_timer_stop(timer);
}

#endif
