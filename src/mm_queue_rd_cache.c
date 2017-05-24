
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

void mm_queuerdcache_init(mm_queuerdcache_t *cache)
{
	mm_list_init(&cache->list);
	cache->count = 0;
}

void mm_queuerdcache_free(mm_queuerdcache_t *cache)
{
	mm_list_t *i, *n;
	mm_list_foreach_safe(&cache->list, i, n) {
		mm_queuerd_t *reader;
		reader = mm_container_of(i, mm_queuerd_t, link);
		mm_queuerd_close(reader);
		free(reader);
	}
}

mm_queuerd_t*
mm_queuerdcache_pop(mm_queuerdcache_t *cache)
{
	mm_queuerd_t *reader = NULL;
	if (cache->count > 0) {
		mm_list_t *first = mm_list_pop(&cache->list);
		cache->count--;
		reader = mm_container_of(first, mm_queuerd_t, link);
		return reader;
	}
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

void mm_queuerdcache_push(mm_queuerdcache_t *cache, mm_queuerd_t *reader)
{
	mm_list_append(&cache->list, &reader->link);
	cache->count++;
}
