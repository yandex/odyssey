#ifndef MM_FD_H_
#define MM_FD_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_fd_t mm_fd_t;

enum {
	MM_R = 1,
	MM_W = 2
};

typedef int (*mm_fd_callback_t)(mm_fd_t*, int);

struct mm_fd_t {
	int              fd;
	int              mask;
	mm_fd_callback_t callback;
	void            *arg;
};

#endif
