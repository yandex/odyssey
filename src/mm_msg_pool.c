
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

void mm_msgpool_init(mm_msgpool_t *pool)
{
	pthread_spin_init(&pool->lock, PTHREAD_PROCESS_PRIVATE);
	mm_list_init(&pool->list);
	pool->count = 0;
}

void mm_msgpool_free(mm_msgpool_t *pool)
{
	mm_list_t *i, *n;
	mm_list_foreach_safe(&pool->list, i, n) {
		mm_msg_t *msg = mm_container_of(i, mm_msg_t, link);
		mm_buf_free(&msg->data);
		free(msg);
	}
	pthread_spin_destroy(&pool->lock);
}

mm_msg_t*
mm_msgpool_pop(mm_msgpool_t *pool)
{
	mm_msg_t *msg = NULL;
	pthread_spin_lock(&pool->lock);
	if (pool->count > 0) {
		mm_list_t *first = mm_list_pop(&pool->list);
		pool->count--;
		pthread_spin_unlock(&pool->lock);
		msg = mm_container_of(first, mm_msg_t, link);
		goto init;
	}
	pthread_spin_unlock(&pool->lock);

	msg = malloc(sizeof(mm_msg_t));
	if (msg == NULL)
		return NULL;
init:
	msg->refs = 0;
	msg->type = 0;
	mm_buf_init(&msg->data);
	mm_list_init(&msg->link);
	return msg;
}

void mm_msgpool_push(mm_msgpool_t *pool, mm_msg_t *msg)
{
	pthread_spin_lock(&pool->lock);
	mm_list_append(&pool->list, &msg->link);
	pool->count++;
	pthread_spin_unlock(&pool->lock);
}
