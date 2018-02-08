#ifndef SHAPITO_CACHE_H
#define SHAPITO_CACHE_H

/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct shapito_cache shapito_cache_t;

struct shapito_cache
{
	pthread_spinlock_t  lock;
	int                 count;
	int                 count_allocated;
	shapito_stream_t   *list;
};

static inline void
shapito_cache_init(shapito_cache_t *cache)
{
	cache->list            = NULL;
	cache->count           = 0;
	cache->count_allocated = 0;
	pthread_spin_init(&cache->lock, PTHREAD_PROCESS_PRIVATE);
}

static inline void
shapito_cache_free(shapito_cache_t *cache)
{
	shapito_stream_t *stream = cache->list;
	while (stream) {
		shapito_stream_t *next = stream->next;
		shapito_stream_free(stream);
		free(stream);
		stream = next;
	}
	pthread_spin_destroy(&cache->lock);
}

static inline shapito_stream_t*
shapito_cache_pop(shapito_cache_t *cache)
{
	pthread_spin_lock(&cache->lock);
	if (cache->count > 0) {
		shapito_stream_t *stream;
		stream = cache->list;
		cache->list = stream->next;
		stream->next = NULL;
		cache->count--;
		pthread_spin_unlock(&cache->lock);
		shapito_stream_reset(stream);
		return stream;
	}
	cache->count_allocated++;
	pthread_spin_unlock(&cache->lock);

	shapito_stream_t *stream;
	stream = malloc(sizeof(shapito_stream_t));
	if (stream == NULL) {
		pthread_spin_lock(&cache->lock);
		cache->count_allocated--;
		pthread_spin_unlock(&cache->lock);
		return NULL;
	}
	shapito_stream_init(stream);
	return stream;
}

static inline void
shapito_cache_push(shapito_cache_t *cache, shapito_stream_t *stream)
{
	pthread_spin_lock(&cache->lock);
	stream->next = cache->list;
	cache->list = stream;
	cache->count++;
	pthread_spin_unlock(&cache->lock);
}

#endif /* SHAPITO_CACHE_H */
