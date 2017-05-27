#ifndef MM_CONDITION_CACHE_H
#define MM_CONDITION_CACHE_H

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_condition_cache mm_condition_cache_t;

struct mm_condition_cache
{
	mm_list_t list;
	int       count;
};

void mm_condition_cache_init(mm_condition_cache_t*);
void mm_condition_cache_free(mm_condition_cache_t*);

mm_condition_t*
mm_condition_cache_pop(mm_condition_cache_t*);

void mm_condition_cache_push(mm_condition_cache_t*, mm_condition_t*);

#endif
