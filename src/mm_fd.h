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

typedef int (*mm_fd_onread_t)(mm_fd_t*);
typedef int (*mm_fd_onwrite_t)(mm_fd_t*);

struct mm_fd_t {
	int              fd;
	int              mask;
	mm_fd_onread_t   on_read;
	void            *on_read_arg;
	mm_fd_onwrite_t  on_write;
	void            *on_write_arg;
};

#endif
