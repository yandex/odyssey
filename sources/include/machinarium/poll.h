#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium/fd.h>

typedef struct mm_pollif mm_pollif_t;
typedef struct mm_poll mm_poll_t;

struct mm_pollif {
	char *name;
	mm_poll_t *(*create)(void);
	void (*free)(mm_poll_t *);
	int (*shutdown)(mm_poll_t *);
	int (*step)(mm_poll_t *, int);
	int (*add)(mm_poll_t *poll, mm_fd_t *fd, mm_fd_callback_t on_read,
		   void *on_read_arg, mm_fd_callback_t on_write,
		   void *on_write_arg, mm_fd_callback_t on_err,
		   void *on_err_arg, mm_fd_callback_t on_close,
		   void *on_close_arg);
	int (*del)(mm_poll_t *, mm_fd_t *);
};

struct mm_poll {
	mm_pollif_t *iface;
};
