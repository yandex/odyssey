#ifndef MM_IO_H
#define MM_IO_H

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_io mm_io_t;

struct mm_io
{
	int         fd;
	mm_fd_t     handle;
	int         attached;
	int         opt_nodelay;
	int         opt_keepalive;
	int         opt_keepalive_delay;
	mm_tlsio_t  tls;
	mm_tls_t   *tls_obj;

	/* connect */
	mm_call_t   connect;
	int         connected;

	/* accept */
	mm_call_t   accept;
	int         accepted;
	int         accept_listen;

	/* read */
	mm_call_t   read;
	char       *read_buf;
	int         read_size;
	int         read_pos;
	int         read_eof;

	mm_buf_t    readahead_buf;
	int         readahead_size;
	int         readahead_pos;
	int         readahead_pos_read;
	int         readahead_status;

	/* write */
	mm_call_t   write;
	char       *write_buf;
	int         write_size;
	int         write_pos;
};

int mm_io_socket_set(mm_io_t*, int);
int mm_io_socket(mm_io_t*, struct sockaddr*);

#endif /* MM_IO_H */
