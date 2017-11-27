
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

void mm_coroutine_cache_init(mm_coroutine_cache_t *cache,
                             int stack_size,
                             int stack_size_guard,
                             int limit)
{
	pthread_spin_init(&cache->lock, PTHREAD_PROCESS_PRIVATE);
	mm_list_init(&cache->list);
	cache->count = 0;
	cache->stack_size = stack_size;
	cache->stack_size_guard = stack_size_guard;
	cache->limit = limit;
}

void mm_coroutine_cache_free(mm_coroutine_cache_t *cache)
{
	mm_list_t *i, *n;
	mm_list_foreach_safe(&cache->list, i, n) {
		mm_coroutine_t *coroutine;
		coroutine = mm_container_of(i, mm_coroutine_t, link);
		mm_coroutine_free(coroutine);
	}
	pthread_spin_destroy(&cache->lock);
}

mm_coroutine_t*
mm_coroutine_cache_pop(mm_coroutine_cache_t *cache)
{
	pthread_spin_lock(&cache->lock);
	if (cache->count > 0) {
		mm_list_t *first = mm_list_pop(&cache->list);
		cache->count--;
		pthread_spin_unlock(&cache->lock);
		mm_coroutine_t *coroutine;
		coroutine = mm_container_of(first, mm_coroutine_t, link);
		return coroutine;
	}
	pthread_spin_unlock(&cache->lock);

	return mm_coroutine_allocate(cache->stack_size, cache->stack_size_guard);
}

void mm_coroutine_cache_push(mm_coroutine_cache_t *cache, mm_coroutine_t *coroutine)
{
	assert(coroutine->state == MM_CFREE);
	pthread_spin_lock(&cache->lock);
	if (cache->count > cache->limit) {
		pthread_spin_unlock(&cache->lock);
		mm_coroutine_free(coroutine);
		return;
	}
	mm_list_append(&cache->list, &coroutine->link);
	cache->count++;
	pthread_spin_unlock(&cache->lock);
}
