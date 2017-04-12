#ifndef MM_POLL_H_
#define MM_POLL_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_pollif_t mm_pollif_t;
typedef struct mm_poll_t   mm_poll_t;

struct mm_pollif_t {
	char      *name;
	mm_poll_t *(*create)(void);
	void       (*free)(mm_poll_t*);
	int        (*shutdown)(mm_poll_t*);
	int        (*step)(mm_poll_t*, int);
	int        (*add)(mm_poll_t*, mm_fd_t*, int);
	int        (*read)(mm_poll_t*, mm_fd_t*, mm_fd_onread_t, void*, int);
	int        (*write)(mm_poll_t*, mm_fd_t*, mm_fd_onwrite_t, void*, int);
	int        (*del)(mm_poll_t*, mm_fd_t*);
};

struct mm_poll_t {
	mm_pollif_t *iface;
};

#endif
