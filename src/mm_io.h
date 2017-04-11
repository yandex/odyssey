#ifndef MM_IO_H_
#define MM_IO_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_io_t mm_io_t;

struct mm_io_t {
	int               fd;
	mm_fd_t           handle;
	int               opt_nodelay;
	int               opt_keepalive;
	int               opt_keepalive_delay;
	/*
	mm_tlsio_t        tls;
	*/
	mm_tls_t         *tls_obj;
	int               errno_;
	mm_t             *machine;

	/* connect */
	mm_timer_t        connect_timer;
	int               connect_timedout;
	int               connected;
	int               connect_status;
	mm_fiber_t       *connect_fiber;

	/* getaddrinfo */
#if 0
	uv_getaddrinfo_t  gai;
	uv_timer_t        gai_timer;
	mm_fiber_t       *gai_fiber;
	int               gai_status;
	int               gai_timedout;
	struct addrinfo  *gai_result;
#endif

	/* accept */
#if 0
	int               accept_status;
	mm_fiber_t       *accept_fiber;
	int               accepted;
#endif
	/* read */
#if 0
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
#endif
	/* write */
#if 0
	uv_write_t        write;
	uv_timer_t        write_timer;
	int               write_timedout;
	int               write_status;
	mm_fiber_t       *write_fiber;
#endif
};

#if 0
static inline void
mm_io_timer_start(mm_timer_t *timer,
                  mm_timer_callback_t callback,
                  uint64_t time_ms)
{
	if (time_ms > 0)
		mm_timer_start(timer, callback, time_ms, 0);
}

static inline void
mm_io_timer_stop(uv_timer_t *timer)
{
	uv_timer_stop(timer);
}
#endif

static inline void
mm_io_set_errno(mm_io_t *io, int rc)
{
	io->errno_ = rc;
}

#endif
