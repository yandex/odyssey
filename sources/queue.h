#ifndef MM_QUEUE_H
#define MM_QUEUE_H

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_queuerd mm_queuerd_t;
typedef struct mm_queue   mm_queue_t;

struct mm_queuerd
{
	mm_event_t  event;
	mm_msg_t   *result;
	mm_list_t   link;
};

struct mm_queue
{
	pthread_spinlock_t lock;
	mm_list_t          msg_list;
	int                msg_list_count;
	mm_list_t          readers;
	int                readers_count;
};

void mm_queue_init(mm_queue_t*);
void mm_queue_free(mm_queue_t*);
void mm_queue_write(mm_queue_t*, mm_msg_t*);
mm_msg_t*
mm_queue_read(mm_queue_t*, uint32_t);

#endif /* MM_QUEUE_H */
