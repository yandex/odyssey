#ifndef MM_MSG_POOL_H_
#define MM_MSG_POOL_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_msgpool_t mm_msgpool_t;

struct mm_msgpool_t {
	pthread_spinlock_t lock;
	mm_list_t          list;
	int                count;
};

void mm_msgpool_init(mm_msgpool_t*);
void mm_msgpool_free(mm_msgpool_t*);

mm_msg_t*
mm_msgpool_pop(mm_msgpool_t*);

void mm_msgpool_push(mm_msgpool_t*, mm_msg_t*);

static inline void
mm_msg_ref(mm_msg_t *msg)
{
	msg->refs++;
}

static inline void
mm_msg_unref(mm_msgpool_t *pool, mm_msg_t *msg)
{
	if (msg->refs > 0) {
		msg->refs--;
		return;
	}
	mm_msgpool_push(pool, msg);
}

#endif
