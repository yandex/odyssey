
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

void mm_queuerdpool_init(mm_queuerdpool_t *pool)
{
	pthread_spin_init(&pool->lock, PTHREAD_PROCESS_PRIVATE);
	mm_list_init(&pool->list);
	pool->count = 0;
}

void mm_queuerdpool_free(mm_queuerdpool_t *pool)
{
	mm_list_t *i, *n;
	mm_list_foreach_safe(&pool->list, i, n) {
		mm_queuerd_t *reader;
		reader = mm_container_of(i, mm_queuerd_t, link);
		mm_queuerd_close(reader);
		free(reader);
	}
	pthread_spin_destroy(&pool->lock);
}

mm_queuerd_t*
mm_queuerdpool_pop(mm_queuerdpool_t *pool)
{
	mm_queuerd_t *reader = NULL;
	pthread_spin_lock(&pool->lock);
	if (pool->count > 0) {
		mm_list_t *first = mm_list_pop(&pool->list);
		pool->count--;
		pthread_spin_unlock(&pool->lock);
		reader = mm_container_of(first, mm_queuerd_t, link);
		return reader;
	}
	pthread_spin_unlock(&pool->lock);

	reader = malloc(sizeof(mm_queuerd_t));
	if (reader == NULL)
		return NULL;
	int rc;
	rc = mm_queuerd_open(reader);
	if (rc == -1) {
		free(reader);
		return NULL;
	}
	return reader;
}

void mm_queuerdpool_push(mm_queuerdpool_t *pool, mm_queuerd_t *reader)
{
	pthread_spin_lock(&pool->lock);
	mm_list_append(&pool->list, &reader->link);
	pool->count++;
	pthread_spin_unlock(&pool->lock);
}
