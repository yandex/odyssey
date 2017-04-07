#ifndef MM_LOOP_H_
#define MM_LOOP_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_loop_t mm_loop_t;

struct mm_loop_t {
	mm_poll_t   *poll;
	mm_timers_t  timers;
};

int mm_loop_init(mm_loop_t*);
int mm_loop_shutdown(mm_loop_t*);
int mm_loop_step(mm_loop_t*);
int mm_loop_add(mm_loop_t*, mm_fd_t*, int);
int mm_loop_modify(mm_loop_t*, mm_fd_t*, int);
int mm_loop_delete(mm_loop_t*, mm_fd_t*);

#endif
