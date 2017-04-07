
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

void mm_timers_init(mm_timers_t *timers)
{
	mm_buf_init(&timers->list);
	timers->time  = 0;
	timers->count = 0;
}

void mm_timers_free(mm_timers_t *timers)
{
	mm_buf_free(&timers->list);
}

static int
mm_timers_cmp(const void *a_ptr, const void *b_ptr)
{
	mm_timer_t *a = (mm_timer_t*)a_ptr;
	mm_timer_t *b = (mm_timer_t*)b_ptr;
	if (a->timeout < b->timeout)
		return 1;
	if (a->timeout > b->timeout)
		return 0;
	if (a->seq < b->seq)
		return 1;
	if (a->seq > b->seq)
		return 0;
	return 0;
}

int mm_timers_add(mm_timers_t *timers, mm_timer_t *timer)
{
	int count = timers->count + 1;
	int rc;
	rc = mm_buf_ensure(&timers->list, count * sizeof(mm_timer_t*));
	if (rc == -1)
		return -1;
	mm_timer_t **list = (mm_timer_t**)timers->list.start;
	list[count - 1] = timer;
	mm_buf_advance(&timers->list, sizeof(mm_timer_t*));
	qsort(list, count, sizeof(mm_timer_t*),
	      mm_timers_cmp);
	timers->count = count;
	return 0;
}

int mm_timers_del(mm_timers_t *timers, mm_timer_t *timer)
{
	assert(timers->count >= 1);
	mm_timer_t **list = (mm_timer_t**)timers->list.start;
	int i = 0;
	int j = 0;
	for (; i < timers->count; i++) {
		if (list[i] == timer)
			continue;
		list[j] = list[i];
		j++;
	}
	timers->list.pos -= sizeof(mm_timer_t*);
	timers->count -= 1;
	qsort(list, timers->count, sizeof(mm_timer_t*),
	      mm_timers_cmp);
	return 0;
}

mm_timer_t*
mm_timers_min(mm_timers_t *timers)
{
	if (timers->count == 0)
		return NULL;
	mm_timer_t **list = (mm_timer_t**)timers->list.start;
	return list[0];
}
