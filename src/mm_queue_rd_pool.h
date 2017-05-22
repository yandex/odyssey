#ifndef MM_QUEUE_RD_POOL_H_
#define MM_QUEUE_RD_POOL_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_queuerdpool_t mm_queuerdpool_t;

struct mm_queuerdpool_t {
	pthread_spinlock_t lock;
	mm_list_t          list;
	int                count;
};

void mm_queuerdpool_init(mm_queuerdpool_t*);
void mm_queuerdpool_free(mm_queuerdpool_t*);

mm_queuerd_t*
mm_queuerdpool_pop(mm_queuerdpool_t*);

void mm_queuerdpool_push(mm_queuerdpool_t*, mm_queuerd_t*);

#endif
