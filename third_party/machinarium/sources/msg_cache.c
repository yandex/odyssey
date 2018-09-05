
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

void mm_msgcache_init(mm_msgcache_t *cache)
{
	pthread_spin_init(&cache->lock, PTHREAD_PROCESS_PRIVATE);
	mm_list_init(&cache->list);
	cache->count = 0;
	cache->count_allocated = 0;
	cache->count_gc = 0;
	cache->size = 0;
	cache->gc_watermark = 512 * 1024;
}

void mm_msgcache_free(mm_msgcache_t *cache)
{
	mm_list_t *i, *n;
	mm_list_foreach_safe(&cache->list, i, n) {
		mm_msg_t *msg = mm_container_of(i, mm_msg_t, link);
		mm_buf_free(&msg->data);
		free(msg);
	}
	pthread_spin_destroy(&cache->lock);
}

void mm_msgcache_stat(mm_msgcache_t *cache,
                      uint64_t *count_allocated,
                      uint64_t *count_gc,
                      uint64_t *count,
                      uint64_t *size)
{
	pthread_spin_lock(&cache->lock);

	*count_allocated = cache->count_allocated;
	*count_gc = cache->count_gc;
	*count = cache->count;
	*size  = cache->size;

	pthread_spin_unlock(&cache->lock);
}

mm_msg_t*
mm_msgcache_pop(mm_msgcache_t *cache)
{
	mm_msg_t *msg = NULL;
	pthread_spin_lock(&cache->lock);
	if (cache->count > 0) {
		mm_list_t *first = mm_list_pop(&cache->list);
		cache->count--;
		msg = mm_container_of(first, mm_msg_t, link);
		cache->size -= mm_buf_size(&msg->data);
		pthread_spin_unlock(&cache->lock);
		goto init;
	}
	cache->count_allocated++;
	pthread_spin_unlock(&cache->lock);

	msg = malloc(sizeof(mm_msg_t));
	if (msg == NULL)
		return NULL;
	mm_buf_init(&msg->data);
init:
	msg->refs = 0;
	msg->type = 0;
	mm_buf_reset(&msg->data);
	mm_list_init(&msg->link);
	return msg;
}

void mm_msgcache_push(mm_msgcache_t *cache, mm_msg_t *msg)
{
	if (mm_buf_size(&msg->data) > cache->gc_watermark) {
		pthread_spin_lock(&cache->lock);
		cache->count_gc++;
		pthread_spin_unlock(&cache->lock);

		mm_buf_free(&msg->data);
		free(msg);
		return;
	}

	pthread_spin_lock(&cache->lock);
	mm_list_append(&cache->list, &msg->link);
	cache->count++;
	cache->size += mm_buf_size(&msg->data);
	pthread_spin_unlock(&cache->lock);
}
