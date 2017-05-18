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
	mm_tlsio_t        tls;
	mm_tls_t         *tls_obj;
	int               errno_;

	/* connect */
	mm_call_t         connect;
	int               connected;

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
	mm_call_t         accept;
	int               accepted;
	int               accept_listen;

	/* read */
	mm_call_t         read;
	char             *read_buf;
	int               read_size;
	int               read_pos;
	int               read_eof;

	mm_buf_t          readahead_buf;
	int               readahead_size;
	int               readahead_pos;
	int               readahead_pos_read;
	int               readahead_status;

	/* write */
	mm_call_t         write;
	char             *write_buf;
	int               write_size;
	int               write_pos;
};

static inline void
mm_io_set_errno(mm_io_t *io, int rc)
{
	io->errno_ = rc;
}

int mm_io_socket_set(mm_io_t*, int);
int mm_io_socket(mm_io_t*, struct sockaddr*);

#endif
