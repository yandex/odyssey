#ifndef MM_QUEUE_H_
#define MM_QUEUE_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_queue_t mm_queue_t;

struct mm_queue_t {
	pthread_spinlock_t lock;
	mm_list_t          msg_list;
	int                msg_list_count;
	mm_list_t          readers;
	int                readers_count;
};

void mm_queue_init(mm_queue_t*);
void mm_queue_free(mm_queue_t*);
void mm_queue_put(mm_queue_t*, mm_msg_t*);
mm_msg_t*
mm_queue_get(mm_queue_t*, mm_queuerd_t*, int);

#endif
