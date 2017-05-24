#ifndef MM_MSG_CACHE_H_
#define MM_MSG_CACHE_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_msgcache_t mm_msgcache_t;

struct mm_msgcache_t {
	pthread_spinlock_t lock;
	mm_list_t          list;
	int                count;
};

void mm_msgcache_init(mm_msgcache_t*);
void mm_msgcache_free(mm_msgcache_t*);

mm_msg_t*
mm_msgcache_pop(mm_msgcache_t*);

void mm_msgcache_push(mm_msgcache_t*, mm_msg_t*);

static inline void
mm_msg_ref(mm_msg_t *msg)
{
	msg->refs++;
}

static inline void
mm_msg_unref(mm_msgcache_t *cache, mm_msg_t *msg)
{
	if (msg->refs > 0) {
		msg->refs--;
		return;
	}
	mm_msgcache_push(cache, msg);
}

#endif
