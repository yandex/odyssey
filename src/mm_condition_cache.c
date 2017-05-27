
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

void mm_condition_cache_init(mm_condition_cache_t *cache)
{
	mm_list_init(&cache->list);
	cache->count = 0;
}

void mm_condition_cache_free(mm_condition_cache_t *cache)
{
	mm_list_t *i, *n;
	mm_list_foreach_safe(&cache->list, i, n) {
		mm_condition_t *condition;
		condition = mm_container_of(i, mm_condition_t, link);
		mm_condition_close(condition);
		free(condition);
	}
}

mm_condition_t*
mm_condition_cache_pop(mm_condition_cache_t *cache)
{
	mm_condition_t *condition = NULL;
	if (cache->count > 0) {
		mm_list_t *first = mm_list_pop(&cache->list);
		cache->count--;
		condition = mm_container_of(first, mm_condition_t, link);
		return condition;
	}
	condition = malloc(sizeof(mm_condition_t));
	if (condition == NULL)
		return NULL;
	int rc;
	rc = mm_condition_open(condition);
	if (rc == -1) {
		free(condition);
		return NULL;
	}
	return condition;
}

void mm_condition_cache_push(mm_condition_cache_t *cache,
                             mm_condition_t *condition)
{
	mm_list_append(&cache->list, &condition->link);
	cache->count++;
}
