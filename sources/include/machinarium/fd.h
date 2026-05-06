#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

/* machinarium file descriptor */

typedef struct mm_fd mm_fd_t;

enum {
	MM_R = (1 << 0),
	MM_W = (1 << 1),
	MM_ERR = (1 << 2),
	MM_CLOSE = (1 << 3)
};

typedef void (*mm_fd_callback_t)(mm_fd_t *);

struct mm_fd {
	int fd;
	int mask;
	mm_fd_callback_t on_read;
	void *on_read_arg;
	mm_fd_callback_t on_write;
	void *on_write_arg;
	mm_fd_callback_t on_err;
	void *on_err_arg;
	mm_fd_callback_t on_close;
	void *on_close_arg;
	int last_event;
};
