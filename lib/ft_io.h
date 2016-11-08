#ifndef FT_IO_H_
#define FT_IO_H_

/*
 * fluent.
 *
 * Cooperative multitasking engine.
*/

typedef struct ftio ftio;

struct ftio {
	uv_os_sock_t  fd;
	uv_tcp_t      handle;
	ft           *f;
	/* connect */
	uv_timer_t    connect_timer;
	uv_connect_t  connect;
	int           connect_timeout;
	int           connected;
	int           connect_status;
	ftfiber      *connect_fiber;
	/* accept */
	int           accept_status;
	ftfiber      *accept_fiber;
	/* read */
	uv_timer_t    read_timer;
	ftbuf         read_buf;
	int           read_size;
	int           read_timeout;
	int           read_eof;
	int           read_status;
	ftfiber      *read_fiber;
	/* write */
	uv_timer_t    write_timer;
	uv_write_t    write;
	int           write_timeout;
	int           write_status;
	ftfiber      *write_fiber;
};

static inline void
ft_io_timer_start(uv_timer_t *timer, uv_timer_cb callback, uint64_t time_ms)
{
	if (time_ms > 0)
		uv_timer_start(timer, callback, time_ms, 0);
}

static inline void
ft_io_timer_stop(uv_timer_t *timer)
{
	uv_timer_stop(timer);
	uv_handle_t *handle = (uv_handle_t*)timer;
	if (uv_is_active(handle))
		uv_close(handle, NULL);
}

#endif
