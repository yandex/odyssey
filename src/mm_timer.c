
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

void mm_timers_init(mm_timers_t *timers)
{
	timers->list = NULL;
	timers->count = 0;
}

void mm_timers_free(mm_timers_t *timers)
{
	if (timers->list)
		free(timers->list);
}

int mm_timers_add(mm_timers_t *timers, mm_timer_t *timer)
{
	int count = timers->count + 1;
	mm_timer_t **list;
	list = realloc(timers->list, count * sizeof(mm_timer_t*));
	if (list == NULL)
		return -1;
	list[count-1] = timer;

	/* sort */

	if (timers->list)
		free(timers->list);
	timers->list = list;
	timers->count = count;
	return 0;
}

int mm_timers_del(mm_timers_t *timers, mm_timer_t *timer)
{
	assert(timers->count >= 1);

	int count = timers->count - 1;
	mm_timer_t **list;
	list = realloc(timers->list, count * sizeof(mm_timer_t*));
	if (list == NULL)
		return -1;

	int i = 0;
	int j = 0;
	for (; i < timers->count; i++) {
		if (timers->list[i] == timer)
			continue;
		list[j] = timers->list[i]; 
		j++;
	}

	/* sort */

	if (timers->list)
		free(timers->list);
	timers->list = list;
	timers->count = count;
	return 0;
}
