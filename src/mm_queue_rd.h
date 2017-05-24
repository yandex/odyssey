#ifndef MM_QUEUE_RD_H
#define MM_QUEUE_RD_H

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_queuerd mm_queuerd_t;

struct mm_queuerd
{
	mm_call_t call;
	mm_fd_t   fd;
	mm_msg_t *result;
	mm_list_t link;
};

int  mm_queuerd_open(mm_queuerd_t*);
void mm_queuerd_close(mm_queuerd_t*);
void mm_queuerd_notify(mm_queuerd_t*);
int  mm_queuerd_wait(mm_queuerd_t*, int);

#endif /* MM_QUEUE_RD_H */
