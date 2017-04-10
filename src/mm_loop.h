#ifndef MM_LOOP_H_
#define MM_LOOP_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_loop_t mm_loop_t;

struct mm_loop_t {
	mm_clock_t  clock;
	mm_idle_t   idle;
	mm_poll_t  *poll;
};

int mm_loop_init(mm_loop_t*);
int mm_loop_shutdown(mm_loop_t*);
int mm_loop_step(mm_loop_t*);
int mm_loop_set_idle(mm_loop_t*, mm_idle_callback_t, void*);
int mm_loop_add(mm_loop_t*, mm_fd_t*, int);
int mm_loop_modify(mm_loop_t*, mm_fd_t*, int);
int mm_loop_delete(mm_loop_t*, mm_fd_t*);

#endif
