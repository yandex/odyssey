#ifndef MM_LOOP_H_
#define MM_LOOP_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_fd_t     mm_fd_t;
typedef struct mm_pollif_t mm_pollif_t;
typedef struct mm_poll_t   mm_poll_t;
typedef struct mm_loop_t   mm_loop_t;

typedef int (*mm_fdevent_t)(mm_fd_t*, int);

struct mm_fd_t {
	int          fd;
	int          type;
	mm_fdevent_t callback;
	void         *arg;
	mm_list_t    link;
};

struct mm_pollif_t {
	char      *name;
	mm_poll_t *(*create)(void);
	void       (*free)(mm_poll_t*);
	int        (*shutdown)(mm_poll_t*);
	int        (*step)(mm_poll_t*);
	int        (*add)(mm_poll_t*, mm_fd_t*, int);
	int        (*modify)(mm_poll_t*, mm_fd_t*, int);
	int        (*del)(mm_poll_t*, mm_fd_t*);
};

struct mm_poll_t {
	mm_pollif_t *iface;
};

struct mm_loop_t {
	mm_poll_t *poll;
};

int mm_loop_init(mm_loop_t*);
int mm_loop_shutdown(mm_loop_t*);
int mm_loop_step(mm_loop_t*);
int mm_loop_add(mm_loop_t*, mm_fd_t*, int);
int mm_loop_modify(mm_loop_t*, mm_fd_t*, int);
int mm_loop_delete(mm_loop_t*, mm_fd_t*);

#endif
