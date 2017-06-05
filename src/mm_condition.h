#ifndef MM_CONDITION_H
#define MM_CONDITION_H

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_condition mm_condition_t;

struct mm_condition
{
	mm_call_t call;
	mm_fd_t   fd;
	mm_list_t link;
};

int  mm_condition_open(mm_condition_t*);
void mm_condition_close(mm_condition_t*);
void mm_condition_signal(int);
int  mm_condition_wait(mm_condition_t*, int);

#endif
